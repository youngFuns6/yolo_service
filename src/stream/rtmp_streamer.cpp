#include "rtmp_streamer.h"
#include <iostream>
#include <optional>
#include <cerrno>
#include <cstring>

namespace detector_service {

/**
 * @brief 将FFmpeg错误码转换为字符串
 */
static std::string avErrorToString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

/**
 * @brief 尝试从编码器上下文更新extradata到流参数
 * @param codec_ctx 编码器上下文
 * @param video_stream 视频流
 * @return 是否成功更新
 */
static bool updateExtradataFromCodec(AVCodecContext* codec_ctx, AVStream* video_stream) {
    if (!codec_ctx || !video_stream) {
        return false;
    }
    
    if (codec_ctx->extradata && codec_ctx->extradata_size > 0) {
        // 释放旧的extradata
        if (video_stream->codecpar->extradata) {
            av_freep(&video_stream->codecpar->extradata);
            video_stream->codecpar->extradata_size = 0;
        }
        
        // 分配新的extradata
        video_stream->codecpar->extradata = (uint8_t*)av_malloc(codec_ctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (video_stream->codecpar->extradata) {
            memcpy(video_stream->codecpar->extradata, codec_ctx->extradata, codec_ctx->extradata_size);
            memset(video_stream->codecpar->extradata + codec_ctx->extradata_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
            video_stream->codecpar->extradata_size = codec_ctx->extradata_size;
            return true;
        }
    }
    return false;
}

/**
 * @brief 从H.264数据包中提取SPS和PPS，并构造extradata
 * @param packet H.264数据包
 * @param sps_data 输出SPS数据（调用者负责释放）
 * @param sps_size 输出SPS大小
 * @param pps_data 输出PPS数据（调用者负责释放）
 * @param pps_size 输出PPS大小
 * @return 是否成功提取
 */
static bool extractSPSPPSFromPacket(const AVPacket* packet, 
                                     uint8_t** sps_data, int* sps_size,
                                     uint8_t** pps_data, int* pps_size) {
    if (!packet || !packet->data || packet->size == 0) {
        return false;
    }
    
    *sps_data = nullptr;
    *sps_size = 0;
    *pps_data = nullptr;
    *pps_size = 0;
    
    // H.264 NAL单元类型：SPS=7, PPS=8
    const uint8_t* data = packet->data;
    int size = packet->size;
    int i = 0;
    
    // 查找NAL单元起始码 (0x00 0x00 0x00 0x01 或 0x00 0x00 0x01)
    while (i < size - 4) {
        // 查找起始码
        if ((data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) ||
            (data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01)) {
            int start_code_len = (data[i+2] == 0x01) ? 3 : 4;
            i += start_code_len;
            
            if (i >= size) break;
            
            // 获取NAL单元类型（低5位）
            uint8_t nal_type = data[i] & 0x1F;
            
            // 找到下一个起始码的位置
            int next_start = size;  // 默认到数据末尾
            int search_start = i + 1;
            while (search_start < size - 3) {
                if ((data[search_start] == 0x00 && data[search_start+1] == 0x00 && 
                     data[search_start+2] == 0x00 && data[search_start+3] == 0x01) ||
                    (data[search_start] == 0x00 && data[search_start+1] == 0x00 && 
                     data[search_start+2] == 0x01)) {
                    next_start = search_start;
                    break;
                }
                search_start++;
            }
            
            // NAL单元大小（不包括起始码）
            int nal_size = next_start - i;
            
            // 验证大小合理性（SPS/PPS通常不会超过几KB）
            if (nal_size <= 0 || nal_size > 65535) {
                std::cerr << "警告: 检测到异常的NAL单元大小: " << nal_size << "，跳过" << std::endl;
                i = (next_start < size) ? next_start : size;
                continue;
            }
            
            if (nal_type == 7 && !*sps_data) {  // SPS
                *sps_data = (uint8_t*)av_malloc(nal_size);
                if (*sps_data) {
                    memcpy(*sps_data, data + i, nal_size);
                    *sps_size = nal_size;
                }
            } else if (nal_type == 8 && !*pps_data) {  // PPS
                *pps_data = (uint8_t*)av_malloc(nal_size);
                if (*pps_data) {
                    memcpy(*pps_data, data + i, nal_size);
                    *pps_size = nal_size;
                }
            }
            
            i = next_start;
        } else {
            i++;
        }
    }
    
    return (*sps_data != nullptr && *sps_size > 0 && *pps_data != nullptr && *pps_size > 0);
}

/**
 * @brief 从SPS/PPS构造H.264 extradata (AVC1格式)
 * @param sps_data SPS数据
 * @param sps_size SPS大小
 * @param pps_data PPS数据
 * @param pps_size PPS大小
 * @param extradata 输出的extradata（调用者负责释放）
 * @param extradata_size 输出的extradata大小
 * @return 是否成功
 */
static bool buildExtradataFromSPSPPS(const uint8_t* sps_data, int sps_size,
                                     const uint8_t* pps_data, int pps_size,
                                     uint8_t** extradata, int* extradata_size) {
    if (!sps_data || sps_size <= 0 || !pps_data || pps_size <= 0) {
        return false;
    }
    
    // 验证SPS/PPS大小合理性（通常SPS < 256字节，PPS < 64字节）
    if (sps_size > 65535 || pps_size > 65535) {
        std::cerr << "错误: SPS/PPS大小异常 (SPS=" << sps_size << ", PPS=" << pps_size << ")" << std::endl;
        return false;
    }
    
    if (sps_size > 1024 || pps_size > 256) {
        std::cerr << "警告: SPS/PPS大小较大 (SPS=" << sps_size << ", PPS=" << pps_size << ")，但继续处理" << std::endl;
    }
    
    // AVC1 extradata格式：
    // 1 byte: configurationVersion (0x01)
    // 1 byte: AVCProfileIndication (从SPS中提取)
    // 1 byte: profile_compatibility (从SPS中提取)
    // 1 byte: AVCLevelIndication (从SPS中提取)
    // 1 byte: lengthSizeMinusOne (0x03，表示4字节长度，因为 0x03 + 1 = 4)
    // 1 byte: numOfSequenceParameterSets (0xE1，表示1个SPS)
    // 2 bytes: SPS长度 (big-endian)
    // SPS数据
    // 1 byte: numOfPictureParameterSets (0x01，表示1个PPS)
    // 2 bytes: PPS长度 (big-endian)
    // PPS数据
    
    int total_size = 8 + sps_size + 3 + pps_size;
    *extradata = (uint8_t*)av_malloc(total_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!*extradata) {
        return false;
    }
    
    uint8_t* out = *extradata;
    
    // configurationVersion
    out[0] = 0x01;
    
    // AVCProfileIndication, profile_compatibility, AVCLevelIndication
    // 从SPS中提取
    // 注意：SPS数据的第一字节是NAL header（包含NAL类型7），实际SPS数据从第二字节开始
    // SPS格式：NAL header (1 byte) + profile_idc (1 byte) + profile_compatibility (1 byte) + level_idc (1 byte) + ...
    if (sps_size >= 4) {
        // 确保第一个字节是SPS的NAL header（类型7）
        uint8_t nal_header = sps_data[0];
        uint8_t nal_type = nal_header & 0x1F;
        if (nal_type == 7) {
            // 正确：SPS数据包含NAL header
            out[1] = sps_data[1];  // profile_idc
            out[2] = sps_data[2];  // profile_compatibility
            out[3] = sps_data[3];  // level_idc
        } else {
            // 异常情况：第一个字节不是SPS NAL header
            std::cerr << "警告: SPS数据格式异常，第一个字节不是SPS NAL header (0x" 
                      << std::hex << (int)nal_header << std::dec << ")" << std::endl;
            // 尝试从第二个字节开始（假设第一个字节被错误包含）
            if (sps_size >= 5) {
                out[1] = sps_data[2];
                out[2] = sps_data[3];
                out[3] = sps_data[4];
            } else {
                // 使用默认值
                out[1] = 0x42;
                out[2] = 0xE0;
                out[3] = 0x1F;
            }
        }
    } else if (sps_size >= 1) {
        // SPS太小，使用默认值
        std::cerr << "警告: SPS数据太小 (" << sps_size << " 字节)，使用默认profile/level" << std::endl;
        out[1] = 0x42;  // baseline profile
        out[2] = 0xE0;  // profile compatibility
        out[3] = 0x1F;  // level 3.1
    } else {
        // 默认值（baseline profile, level 3.1）
        out[1] = 0x42;  // baseline profile
        out[2] = 0xE0;  // profile compatibility
        out[3] = 0x1F;  // level 3.1
    }
    
    // lengthSizeMinusOne (0x03 = 4字节长度，因为 0x03 + 1 = 4)
    // 注意：必须是 0x03，不能是 0xFF！
    out[4] = 0x03;
    
    // numOfSequenceParameterSets (0xE1 = 1个SPS)
    out[5] = 0xE1;
    
    // SPS长度 (big-endian)
    out[6] = (sps_size >> 8) & 0xFF;
    out[7] = sps_size & 0xFF;
    
    // SPS数据
    memcpy(out + 8, sps_data, sps_size);
    
    // numOfPictureParameterSets (0x01 = 1个PPS)
    out[8 + sps_size] = 0x01;
    
    // PPS长度 (big-endian)
    out[8 + sps_size + 1] = (pps_size >> 8) & 0xFF;
    out[8 + sps_size + 2] = pps_size & 0xFF;
    
    // PPS数据
    memcpy(out + 8 + sps_size + 3, pps_data, pps_size);
    
    // 填充零
    memset(out + total_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    
    *extradata_size = total_size;
    
    // 验证extradata格式（调试用）
    if (total_size < 8) {
        std::cerr << "错误: extradata大小异常: " << total_size << std::endl;
        av_freep(extradata);
        return false;
    }
    
    // 验证关键字段
    if (out[4] != 0x03) {
        std::cerr << "错误: lengthSizeMinusOne不是0x03: 0x" << std::hex << (int)out[4] << std::dec << std::endl;
        av_freep(extradata);
        return false;
    }
    
    // 验证SPS/PPS长度字段
    uint16_t sps_len = (out[6] << 8) | out[7];
    if (sps_len != sps_size) {
        std::cerr << "错误: SPS长度不匹配 (extradata中=" << sps_len << ", 实际=" << sps_size << ")" << std::endl;
        av_freep(extradata);
        return false;
    }
    
    uint16_t pps_len = (out[8 + sps_size + 1] << 8) | out[8 + sps_size + 2];
    if (pps_len != pps_size) {
        std::cerr << "错误: PPS长度不匹配 (extradata中=" << pps_len << ", 实际=" << pps_size << ")" << std::endl;
        av_freep(extradata);
        return false;
    }
    
    return true;
}

bool RTMPStreamer::initialize(const std::string& rtmp_url, int width, int height, 
                              int fps, std::optional<int> bitrate) {
    // 如果已经初始化，先清理
    if (isInitialized()) {
        close();
    }
    
    // 初始化FFmpeg网络库
    avformat_network_init();
    
    // 创建输出上下文
    int ret = avformat_alloc_output_context2(&fmt_ctx, nullptr, "flv", rtmp_url.c_str());
    if (ret < 0 || !fmt_ctx) {
        std::cerr << "无法创建输出格式上下文: " << avErrorToString(ret) << std::endl;
        return false;
    }
    
    // 查找H.264编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "找不到H.264编码器" << std::endl;
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 创建编码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "无法分配编码器上下文" << std::endl;
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 设置编码参数
    codec_ctx->codec_id = AV_CODEC_ID_H264;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = {1, fps};
    codec_ctx->framerate = {fps, 1};
    codec_ctx->gop_size = fps;  // 关键帧间隔：1秒（更频繁的关键帧可能有助于RTMP兼容性）
    codec_ctx->max_b_frames = 0;  // RTMP/FLV不支持B帧，设置为0
    
    // 设置比特率
    if (bitrate.has_value() && bitrate.value() > 0) {
        codec_ctx->bit_rate = bitrate.value();
    } else {
        // 默认比特率：根据分辨率估算
        codec_ctx->bit_rate = width * height * fps / 10;
    }
    
    // 设置H.264预设（快速编码，低延迟）
    AVDictionary* opts = nullptr;
    av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(codec_ctx->priv_data, "profile", "baseline", 0);
    // 对于x264编码器，强制在每个关键帧中包含SPS/PPS（这对RTMP/FLV很重要）
    // 注意：如果使用其他编码器，此选项可能无效，但不会导致错误
    if (codec->id == AV_CODEC_ID_H264) {
        int ret_opt = av_opt_set(codec_ctx->priv_data, "x264-params", "repeat_headers=1", 0);
        if (ret_opt < 0) {
            // 选项设置失败（可能是非x264编码器），继续使用默认设置
            std::cout << "注意: 无法设置x264-params，将使用编码器默认设置" << std::endl;
        }
    }
    
    // 打开编码器
    ret = avcodec_open2(codec_ctx, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        std::cerr << "无法打开编码器: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 创建视频流
    video_stream = avformat_new_stream(fmt_ctx, codec);
    if (!video_stream) {
        std::cerr << "无法创建视频流" << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 设置流的时间基与编码器一致
    video_stream->time_base = codec_ctx->time_base;
    
    // 复制编码器参数到流
    // 注意：此时编码器可能还没有extradata（SPS/PPS），需要在编码第一帧后更新
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0) {
        std::cerr << "无法复制编码器参数: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 检查extradata是否存在
    if (!codec_ctx->extradata || codec_ctx->extradata_size == 0) {
        std::cout << "警告: 编码器extradata为空，将在第一帧编码后更新" << std::endl;
    } else {
        std::cout << "编码器extradata大小: " << codec_ctx->extradata_size << " 字节" << std::endl;
    }
    
    // 创建SWS上下文用于颜色空间转换（BGR24 -> YUV420P）
    sws_ctx = sws_getContext(width, height, AV_PIX_FMT_BGR24,
                             width, height, AV_PIX_FMT_YUV420P,
                             SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        std::cerr << "无法创建颜色空间转换上下文" << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 打开输出URL
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        std::cout << "正在连接到RTMP服务器: " << rtmp_url << std::endl;
        ret = avio_open(&fmt_ctx->pb, rtmp_url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "无法打开输出URL: " << avErrorToString(ret) << std::endl;
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
            avcodec_free_context(&codec_ctx);
            avformat_free_context(fmt_ctx);
            fmt_ctx = nullptr;
            return false;
        }
        std::cout << "RTMP服务器连接成功" << std::endl;
    }
    
    // 如果编码器还没有extradata，先编码一个dummy帧来生成SPS/PPS
    // 这对于FLV/RTMP格式很重要，因为头部需要extradata
    if (!codec_ctx->extradata || codec_ctx->extradata_size == 0) {
        std::cout << "编码器extradata为空，先编码一个dummy帧来生成SPS/PPS..." << std::endl;
        
        // 创建一个dummy帧
        AVFrame *dummy_frame = av_frame_alloc();
        if (dummy_frame) {
            dummy_frame->format = AV_PIX_FMT_YUV420P;
            dummy_frame->width = width;
            dummy_frame->height = height;
            dummy_frame->pict_type = AV_PICTURE_TYPE_I;  // 强制I帧
            
            if (av_frame_get_buffer(dummy_frame, 32) >= 0) {
                // 填充黑色帧
                memset(dummy_frame->data[0], 0, dummy_frame->linesize[0] * height);
                memset(dummy_frame->data[1], 128, dummy_frame->linesize[1] * height / 2);
                memset(dummy_frame->data[2], 128, dummy_frame->linesize[2] * height / 2);
                
                dummy_frame->pts = 0;
                
                // 发送到编码器
                ret = avcodec_send_frame(codec_ctx, dummy_frame);
                if (ret >= 0) {
                    // 接收包（但不写入，只是为了生成extradata）
                    // 注意：不要刷新编码器，保持编码器状态以便后续帧可以正常发送
                    AVPacket *dummy_pkt = av_packet_alloc();
                    if (dummy_pkt) {
                        bool found_sps_pps = false;
                        // 接收所有可用的包，直到编码器返回EAGAIN（需要更多输入）
                        while (true) {
                            ret = avcodec_receive_packet(codec_ctx, dummy_pkt);
                            if (ret == 0) {
                                // 如果extradata仍然为空，尝试从dummy帧包中提取SPS/PPS
                                if ((!codec_ctx->extradata || codec_ctx->extradata_size == 0) && !found_sps_pps) {
                                    uint8_t* sps_data = nullptr;
                                    int sps_size = 0;
                                    uint8_t* pps_data = nullptr;
                                    int pps_size = 0;
                                    
                                    if (extractSPSPPSFromPacket(dummy_pkt, &sps_data, &sps_size, &pps_data, &pps_size)) {
                                        uint8_t* extradata = nullptr;
                                        int extradata_size = 0;
                                        
                                        if (buildExtradataFromSPSPPS(sps_data, sps_size, pps_data, pps_size, 
                                                                     &extradata, &extradata_size)) {
                                            // 更新编码器上下文的extradata
                                            if (codec_ctx->extradata) {
                                                av_freep(&codec_ctx->extradata);
                                                codec_ctx->extradata_size = 0;
                                            }
                                            codec_ctx->extradata = extradata;
                                            codec_ctx->extradata_size = extradata_size;
                                            found_sps_pps = true;
                                            std::cout << "已从dummy帧包中提取extradata (SPS/PPS): " 
                                                      << extradata_size << " 字节" << std::endl;
                                        }
                                        
                                        // 释放临时数据
                                        if (sps_data) av_freep(&sps_data);
                                        if (pps_data) av_freep(&pps_data);
                                    }
                                }
                                // 继续接收其他包
                                av_packet_unref(dummy_pkt);
                            } else if (ret == AVERROR(EAGAIN)) {
                                // 编码器需要更多输入，这是正常状态，可以继续发送新帧
                                break;
                            } else if (ret == AVERROR_EOF) {
                                // 编码器已结束（不应该发生，因为我们没有刷新）
                                std::cerr << "警告: 编码器意外返回EOF" << std::endl;
                                break;
                            } else {
                                // 其他错误
                                std::cerr << "接收dummy帧包时出错: " << avErrorToString(ret) << std::endl;
                                break;
                            }
                        }
                        av_packet_free(&dummy_pkt);
                    }
                } else {
                    std::cerr << "发送dummy帧到编码器失败: " << avErrorToString(ret) << std::endl;
                }
            }
            av_frame_free(&dummy_frame);
        }
        
        // 更新extradata到流参数
        // 优先使用编码器生成的extradata，这是最可靠的方式
        if (updateExtradataFromCodec(codec_ctx, video_stream)) {
            std::cout << "已生成并设置extradata (SPS/PPS): " << codec_ctx->extradata_size << " 字节" << std::endl;
            
            // 验证extradata格式
            if (codec_ctx->extradata_size >= 8) {
                uint8_t length_size = codec_ctx->extradata[4];
                if (length_size != 0x03) {
                    std::cerr << "警告: 编码器生成的extradata中lengthSizeMinusOne不是0x03: 0x" 
                              << std::hex << (int)length_size << std::dec << std::endl;
                    std::cerr << "这可能导致RTMP服务器解析错误，尝试修复..." << std::endl;
                    // 修复lengthSizeMinusOne
                    video_stream->codecpar->extradata[4] = 0x03;
                    codec_ctx->extradata[4] = 0x03;
                    std::cout << "已修复lengthSizeMinusOne为0x03" << std::endl;
                }
            }
        } else {
            std::cerr << "警告: dummy帧编码后仍未生成extradata，将在第一帧关键帧后尝试更新" << std::endl;
        }
    }
    
    // 检查extradata是否已设置（对RTMP/FLV格式很重要）
    if (!video_stream->codecpar->extradata || video_stream->codecpar->extradata_size == 0) {
        std::cerr << "警告: 写入头部前extradata为空，RTMP服务器可能无法正确解析流" << std::endl;
        std::cerr << "将在第一帧关键帧后尝试从包中提取SPS/PPS" << std::endl;
    }
    
    // 设置FLV/RTMP选项
    AVDictionary* format_opts = nullptr;
    av_dict_set(&format_opts, "flvflags", "no_duration_filesize", 0);
    av_dict_set(&format_opts, "rtmp_live", "live", 0);
    av_dict_set_int(&format_opts, "buffer_size", 65536, 0);  // 64KB缓冲区
    av_dict_set_int(&format_opts, "rtmp_timeout", 30000, 0);  // 增加超时时间到30秒
    av_dict_set(&format_opts, "fflags", "flush_packets", 0);  // 立即刷新包
    // 添加额外的RTMP选项以提高兼容性和稳定性
    av_dict_set(&format_opts, "rtmp_playpath", "", 0);  // 清空playpath
    av_dict_set(&format_opts, "rtmp_app", "", 0);  // 让FFmpeg自动解析
    av_dict_set(&format_opts, "rtmp_flashver", "FMLE/3.0", 0);  // 设置Flash版本以提高兼容性
    
    // 写入头部（此时extradata应该已经存在）
    ret = avformat_write_header(fmt_ctx, &format_opts);
    av_dict_free(&format_opts);
    if (ret < 0) {
        std::cerr << "无法写入文件头: " << avErrorToString(ret) << std::endl;
        if (fmt_ctx->pb) {
            avio_closep(&fmt_ctx->pb);
        }
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    frame_count = 0;
    std::cout << "RTMP推流初始化成功 (尺寸: " << width << "x" << height 
              << ", 帧率: " << fps << ", 比特率: " << (codec_ctx->bit_rate / 1000) << "kbps)" << std::endl;
    
    return true;
}

bool RTMPStreamer::pushFrame(const cv::Mat& frame) {
    if (!isInitialized()) {
        std::cerr << "RTMP推流未初始化" << std::endl;
        return false;
    }
    
    // 验证帧格式
    if (frame.empty() || frame.type() != CV_8UC3) {
        std::cerr << "无效的视频帧格式" << std::endl;
        return false;
    }
    
    // 验证帧尺寸
    if (frame.cols != codec_ctx->width || frame.rows != codec_ctx->height) {
        std::cerr << "视频帧尺寸不匹配: 期望 " << codec_ctx->width << "x" << codec_ctx->height
                  << ", 实际 " << frame.cols << "x" << frame.rows << std::endl;
        return false;
    }
    
    // 创建AVFrame
    AVFrame *av_frame = av_frame_alloc();
    if (!av_frame) {
        std::cerr << "无法分配AVFrame" << std::endl;
        return false;
    }
    
    av_frame->format = AV_PIX_FMT_YUV420P;
    av_frame->width = codec_ctx->width;
    av_frame->height = codec_ctx->height;
    
    // 第一帧必须是关键帧（I帧），RTMP/FLV要求
    bool is_first_frame = (frame_count == 0);
    if (is_first_frame) {
        av_frame->pict_type = AV_PICTURE_TYPE_I;
    }
    
    // 设置帧的PTS（使用当前frame_count，然后递增）
    int64_t current_frame_pts = frame_count;
    av_frame->pts = current_frame_pts;
    frame_count++;
    
    // 分配YUV缓冲区
    int ret = av_frame_get_buffer(av_frame, 32);
    if (ret < 0) {
        std::cerr << "无法为AVFrame分配缓冲区: " << avErrorToString(ret) << std::endl;
        av_frame_free(&av_frame);
        return false;
    }
    
    // 将OpenCV BGR转换为YUV420P
    const uint8_t* src_data[1] = { frame.data };
    int src_linesize[1] = { static_cast<int>(frame.step) };
    
    sws_scale(sws_ctx, src_data, src_linesize, 0, frame.rows,
              av_frame->data, av_frame->linesize);
    
    // 编码帧
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        std::cerr << "无法分配AVPacket" << std::endl;
        av_frame_free(&av_frame);
        return false;
    }
    
    // 发送帧到编码器
    ret = avcodec_send_frame(codec_ctx, av_frame);
    if (ret < 0) {
        // 如果是EOF错误，编码器已被刷新，无法接受新帧
        // 这通常不应该发生，如果发生说明编码器状态异常
        if (ret == AVERROR_EOF) {
            std::cerr << "发送帧到编码器失败: End of file (编码器处于刷新状态，无法接受新帧)" << std::endl;
            std::cerr << "这通常表示编码器已被意外刷新，需要重新初始化推流" << std::endl;
        } else {
            std::cerr << "发送帧到编码器失败: " << avErrorToString(ret) << std::endl;
        }
        av_frame_free(&av_frame);
        av_packet_free(&pkt);
        return false;
    }
    
    // 接收编码后的包（可能需要循环接收多个包，特别是关键帧）
    bool packet_written = false;
    bool first_keyframe_sent = false;
    int packet_count = 0;
    while (true) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == 0) {
            // 跳过空包或无效包 - 这是第一道防线
            if (pkt->size == 0 || !pkt->data) {
                std::cerr << "[跳过] 收到空包或无效包 (size=" << pkt->size 
                          << ", data=" << (pkt->data ? "有效" : "NULL") << ")，跳过" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            
            packet_count++;
            bool is_keyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
            
            // 对于第一帧，只写入关键帧包，跳过非关键帧包
            if (is_first_frame && !first_keyframe_sent) {
                if (!is_keyframe) {
                    // 跳过非关键帧包，继续等待关键帧
                    std::cerr << "第一帧收到非关键帧包，跳过等待关键帧..." << std::endl;
                    av_packet_unref(pkt);
                    continue;
                } else {
                    // std::cout << "第一帧收到关键帧包，准备发送" << std::endl;
                    
                    // 如果extradata仍然为空，尝试从关键帧包中提取SPS/PPS
                    if (!video_stream->codecpar->extradata || video_stream->codecpar->extradata_size == 0) {
                        uint8_t* sps_data = nullptr;
                        int sps_size = 0;
                        uint8_t* pps_data = nullptr;
                        int pps_size = 0;
                        
                        if (extractSPSPPSFromPacket(pkt, &sps_data, &sps_size, &pps_data, &pps_size)) {
                            uint8_t* extradata = nullptr;
                            int extradata_size = 0;
                            
                            if (buildExtradataFromSPSPPS(sps_data, sps_size, pps_data, pps_size, 
                                                         &extradata, &extradata_size)) {
                                // 更新流的extradata
                                if (video_stream->codecpar->extradata) {
                                    av_freep(&video_stream->codecpar->extradata);
                                    video_stream->codecpar->extradata_size = 0;
                                }
                                video_stream->codecpar->extradata = extradata;
                                video_stream->codecpar->extradata_size = extradata_size;
                                
                                // 同时更新编码器上下文的extradata（如果可能）
                                if (codec_ctx->extradata) {
                                    av_freep(&codec_ctx->extradata);
                                    codec_ctx->extradata_size = 0;
                                }
                                codec_ctx->extradata = (uint8_t*)av_malloc(extradata_size);
                                if (codec_ctx->extradata) {
                                    memcpy(codec_ctx->extradata, extradata, extradata_size);
                                    codec_ctx->extradata_size = extradata_size;
                                }
                                
                                std::cout << "已从关键帧包中提取并设置extradata (SPS/PPS): " 
                                          << extradata_size << " 字节" << std::endl;
                            } else {
                                std::cerr << "警告: 无法从SPS/PPS构造extradata" << std::endl;
                            }
                            
                            // 释放临时数据
                            if (sps_data) av_freep(&sps_data);
                            if (pps_data) av_freep(&pps_data);
                        } else {
                            std::cerr << "警告: 无法从关键帧包中提取SPS/PPS" << std::endl;
                        }
                    }
                }
            }
            
            // 设置时间戳 - 在转换前检查并修复
            // 如果编码器没有设置时间戳，使用当前帧的PTS
            int64_t original_pts = pkt->pts;
            int64_t original_dts = pkt->dts;
            
            if (pkt->pts == AV_NOPTS_VALUE || pkt->pts < 0) {
                pkt->pts = current_frame_pts;
            }
            if (pkt->dts == AV_NOPTS_VALUE || pkt->dts < 0) {
                if (pkt->pts != AV_NOPTS_VALUE && pkt->pts >= 0) {
                    pkt->dts = pkt->pts;
                } else {
                    pkt->dts = current_frame_pts;
                }
            }
            
            // 验证时间戳在转换前是有效的
            if (pkt->pts == AV_NOPTS_VALUE || pkt->pts < 0 || pkt->dts == AV_NOPTS_VALUE || pkt->dts < 0) {
                std::cerr << "错误: 无法修复包的时间戳 (原始PTS=" << original_pts 
                          << ", 原始DTS=" << original_dts << ", 当前帧PTS=" << current_frame_pts 
                          << ")，跳过此包" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            
            // 将时间戳从编码器时间基转换到流时间基
            av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
            pkt->stream_index = video_stream->index;
            
            // 确保 DTS <= PTS（对于没有B帧的情况）
            if (pkt->dts > pkt->pts) {
                pkt->dts = pkt->pts;
            }
            
            // 最终检查：如果时间戳在转换后仍然无效，跳过这个包
            if (pkt->pts == AV_NOPTS_VALUE || pkt->pts < 0 || pkt->dts == AV_NOPTS_VALUE || pkt->dts < 0) {
                std::cerr << "警告: 包的时间戳在转换后仍然无效 (PTS=" << pkt->pts 
                          << ", DTS=" << pkt->dts << ")，跳过此包" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            
            // 最终验证：确保包有效后再写入
            if (pkt->size == 0 || !pkt->data) {
                // std::cerr << "错误: 准备写入的包是空的 (size=" << pkt->size << ")，跳过" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            
            // 检查连接是否仍然有效
            if (!fmt_ctx || !fmt_ctx->pb) {
                std::cerr << "RTMP连接已断开" << std::endl;
                av_packet_unref(pkt);
                av_frame_free(&av_frame);
                av_packet_free(&pkt);
                return false;
            }
            
            // 写入前的最终验证 - 确保包仍然有效
            if (pkt->size == 0 || !pkt->data || pkt->pts == AV_NOPTS_VALUE || pkt->pts < 0 || 
                pkt->dts == AV_NOPTS_VALUE || pkt->dts < 0) {
                std::cerr << "[最终检查失败] 包在写入前验证失败 (size=" << pkt->size 
                          << ", data=" << (pkt->data ? "有效" : "NULL")
                          << ", PTS=" << pkt->pts << ", DTS=" << pkt->dts << ")，跳过" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            
            // 在写入前保存包信息（用于错误报告）
            int64_t saved_pts = pkt->pts;
            int64_t saved_dts = pkt->dts;
            int saved_size = pkt->size;
            
            // 在写入前记录调试信息
            if (!is_first_frame && packet_count == 1) {
                // std::cout << "准备写入第" << frame_count << "帧: 包大小=" << pkt->size 
                //           << ", PTS=" << pkt->pts << ", DTS=" << pkt->dts 
                //           << ", 关键帧=" << (is_keyframe ? "是" : "否") << std::endl;
            }
            
            // 写入包
            // 对于RTMP，使用av_interleaved_write_frame以确保正确的时序和交错
            // 这对于RTMP服务器正确处理流很重要
            ret = av_interleaved_write_frame(fmt_ctx, pkt);
            
            if (ret < 0) {
                // 检查是否是连接问题
                if (ret == AVERROR(EPIPE) || ret == AVERROR(ECONNRESET) || 
                    ret == AVERROR(ETIMEDOUT) || ret == AVERROR(EIO)) {
                    std::cerr << "RTMP连接已断开: " << avErrorToString(ret) 
                              << " (写入前: size=" << saved_size 
                              << ", PTS=" << saved_pts << ", DTS=" << saved_dts
                              << ", 关键帧=" << (is_keyframe ? "是" : "否")
                              << ", 帧计数=" << frame_count << ")" << std::endl;
                    
                    // 尝试刷新缓冲区，可能能恢复连接
                    if (fmt_ctx && fmt_ctx->pb) {
                        avio_flush(fmt_ctx->pb);
                    }
                } else {
                    std::cerr << "写入数据包失败: " << avErrorToString(ret) 
                              << " (写入前: size=" << saved_size 
                              << ", PTS=" << saved_pts << ", DTS=" << saved_dts
                              << ", 关键帧=" << (is_keyframe ? "是" : "否")
                              << ", 帧计数=" << frame_count << ")" << std::endl;
                }
                av_packet_unref(pkt);
                av_frame_free(&av_frame);
                av_packet_free(&pkt);
                return false;
            }
            
            // 定期刷新缓冲区以提高稳定性（每10帧或关键帧后）
            if (is_keyframe || (frame_count % 10 == 0)) {
                if (fmt_ctx && fmt_ctx->pb) {
                    avio_flush(fmt_ctx->pb);
                }
            }
            
            packet_written = true;
            if (is_keyframe) {
                first_keyframe_sent = true;
                if (is_first_frame) {
                    std::cout << "第一帧（关键帧）已成功写入" << std::endl;
                    
                    // 第一帧关键帧后，再次尝试从编码器上下文更新extradata（作为备用）
                    // 注意：我们已经在上面的代码中尝试从包中提取SPS/PPS
                    if (updateExtradataFromCodec(codec_ctx, video_stream)) {
                        std::cout << "已从编码器上下文更新流extradata (SPS/PPS): " << codec_ctx->extradata_size << " 字节" << std::endl;
                    } else if (!video_stream->codecpar->extradata || video_stream->codecpar->extradata_size == 0) {
                        // 如果编码器也没有extradata，且我们之前提取也失败了
                        std::cerr << "警告: 第一帧关键帧后，extradata仍为空（编码器未生成且提取失败）" << std::endl;
                        std::cerr << "这可能导致RTMP服务器无法正确解析流，连接可能不稳定" << std::endl;
                    } else {
                        // extradata已通过包提取设置，这是正常的
                        std::cout << "extradata已通过包提取设置" << std::endl;
                    }
                }
            }
            
            av_packet_unref(pkt);
        } else if (ret == AVERROR(EAGAIN)) {
            // 编码器需要更多输入
            // 对于第一帧，如果还没收到关键帧，尝试刷新编码器
            if (is_first_frame && !first_keyframe_sent) {
                // 发送NULL帧来刷新编码器，强制输出关键帧
                ret = avcodec_send_frame(codec_ctx, nullptr);
                if (ret < 0 && ret != AVERROR_EOF) {
                    std::cerr << "刷新编码器失败: " << avErrorToString(ret) << std::endl;
                    break;
                }
                // 继续循环接收包
                continue;
            }
            break;
        } else if (ret == AVERROR_EOF) {
            // 编码器已刷新完成
            break;
        } else {
            // 其他错误
            std::cerr << "接收编码数据包失败: " << avErrorToString(ret) << std::endl;
            break;
        }
    }
    
    // 对于第一帧，必须发送关键帧，否则返回失败
    if (is_first_frame && !first_keyframe_sent) {
        std::cerr << "错误: 第一帧未产生关键帧，无法继续推流" << std::endl;
        av_frame_free(&av_frame);
        av_packet_free(&pkt);
        return false;
    }
    
    av_frame_free(&av_frame);
    av_packet_free(&pkt);
    return true;
}

void RTMPStreamer::close() {
    // 写入尾部
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
    }
    
    // 清理资源
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    
    if (fmt_ctx && !(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&fmt_ctx->pb);
    }
    
    if (fmt_ctx) {
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
    }
    
    video_stream = nullptr;
    frame_count = 0;
    
    avformat_network_deinit();
    
    std::cout << "RTMP推流已关闭" << std::endl;
}

} // namespace detector_service

