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

bool RTMPStreamer::initialize(const std::string& rtmp_url, int width, int height, 
                              int fps, std::optional<int> bitrate) {
    // 如果已经初始化，先清理
    if (isInitialized()) {
        close();
    }
    
    std::cout << "使用RTMP URL: " << rtmp_url << std::endl;
    
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
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = AVRational{1, fps};  // 时间基：1/fps秒
    codec_ctx->framerate = AVRational{fps, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // 设置比特率 - 根据分辨率自动计算合适的比特率
    if (bitrate.has_value() && bitrate.value() > 0) {
        codec_ctx->bit_rate = bitrate.value();
    } else {
        // 根据分辨率自动计算比特率（平衡画质和流畅度）
        int pixels = width * height;
        if (pixels >= 1920 * 1080) {
            codec_ctx->bit_rate = 2500000;  // 1080p: 2.5Mbps
        } else if (pixels >= 1280 * 720) {
            codec_ctx->bit_rate = 1800000;  // 720p: 1.8Mbps
        } else if (pixels >= 854 * 480) {
            codec_ctx->bit_rate = 1000000;  // 480p: 1Mbps
        } else {
            codec_ctx->bit_rate = 700000;   // 更小分辨率: 700kbps
        }
        std::cout << "自动计算比特率: " << (codec_ctx->bit_rate / 1000) << "kbps (分辨率: " 
                  << width << "x" << height << ")" << std::endl;
    }
    
    // *** GOP设置：2秒一个关键帧，确保P帧正常生成 ***
    codec_ctx->gop_size = fps * 2;  // 2秒一个关键帧（对25fps就是50帧）
    codec_ctx->max_b_frames = 0;  // 不使用B帧，降低延迟
    
    // 设置最小关键帧间隔（确保不会过于频繁生成关键帧）
    codec_ctx->keyint_min = fps;  // 最少1秒间隔
    
    // 强制关键帧间隔，防止编码器自动插入额外关键帧
    codec_ctx->refs = 1;  // 减少参考帧数量，降低延迟
    
    // *** 关键：设置编码器输出时间戳 ***
    codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    
    // 强制编码器使用输入的PTS
    codec_ctx->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
    
    // *** 关键：对于FLV/RTMP，必须设置GLOBAL_HEADER标志 ***
    // 这会让编码器立即生成SPS/PPS并放入extradata
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Profile和level设置
    av_opt_set(codec_ctx->priv_data, "profile", "main", 0);  // 使用main profile
    av_opt_set(codec_ctx->priv_data, "level", "4.0", 0);  // H.264 level
    
    // 重复SPS/PPS以确保每个关键帧都包含
    av_opt_set(codec_ctx->priv_data, "repeat-headers", "1", 0);
    
    // *** 优化编码器参数：确保流畅稳定的推流 ***
    av_opt_set(codec_ctx->priv_data, "preset", "veryfast", 0);  // 使用veryfast预设，平衡速度和质量
    av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);  // 低延迟调优
    
    // 线程设置
    av_opt_set(codec_ctx->priv_data, "threads", "2", 0);  // 减少线程数，避免编码时间波动
    av_opt_set(codec_ctx->priv_data, "thread_type", "slice", 0);  // 使用slice级别多线程
    
    // 码率控制：使用CBR模式，保证稳定的码率和流畅度
    av_opt_set(codec_ctx->priv_data, "nal-hrd", "cbr", 0);  // 使用CBR模式
    av_opt_set(codec_ctx->priv_data, "bufsize", std::to_string(codec_ctx->bit_rate).c_str(), 0);  // VBV缓冲区等于码率
    av_opt_set(codec_ctx->priv_data, "maxrate", std::to_string(codec_ctx->bit_rate).c_str(), 0);  // 最大码率等于目标码率
    av_opt_set(codec_ctx->priv_data, "minrate", std::to_string(codec_ctx->bit_rate).c_str(), 0);  // 最小码率等于目标码率
    
    // GOP和场景切换设置
    av_opt_set(codec_ctx->priv_data, "sc_threshold", "0", 0);  // 禁用场景切换检测，避免意外插入关键帧
    av_opt_set(codec_ctx->priv_data, "forced-idr", "1", 0);  // 强制使用IDR关键帧
    av_opt_set(codec_ctx->priv_data, "x264-params", "keyint=" + std::to_string(codec_ctx->gop_size) + ":min-keyint=" + std::to_string(codec_ctx->keyint_min), 0);  // 强制GOP大小
    
    // 其他优化
    av_opt_set(codec_ctx->priv_data, "rc-lookahead", "0", 0);  // 禁用前瞻，减少延迟
    av_opt_set(codec_ctx->priv_data, "me_range", "16", 0);  // 限制运动搜索范围，提升编码速度
    av_opt_set(codec_ctx->priv_data, "sliced-threads", "0", 0);  // 禁用切片线程，减少延迟抖动

    // 打开编码器
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "无法打开编码器: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 验证extradata是否生成
    if (codec_ctx->extradata_size > 0) {
        std::cout << "编码器extradata已生成，大小=" << codec_ctx->extradata_size << " 字节" << std::endl;
    } else {
        std::cout << "警告: 编码器未生成extradata，可能需要编码一帧后才会生成" << std::endl;
    }

    // 创建视频流
    video_stream = avformat_new_stream(fmt_ctx, nullptr);  // 不传递codec，让muxer自己处理
    if (!video_stream) {
        std::cerr << "无法创建视频流" << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }

    // *** 关键：设置流的时间基为1/1000（毫秒），这是RTMP标准 ***
    video_stream->time_base = AVRational{1, 1000};
    video_stream->avg_frame_rate = codec_ctx->framerate;
    
    // *** 关键修复：从编码器上下文复制参数到流，这会包含extradata（SPS/PPS）***
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0) {
        std::cerr << "无法复制编码器参数: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    std::cout << "流参数已设置: codec_id=" << video_stream->codecpar->codec_id 
              << ", extradata_size=" << video_stream->codecpar->extradata_size << std::endl;

    // 创建SWS上下文
    sws_ctx = sws_getContext(width, height, AV_PIX_FMT_BGR24,
                             width, height, AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        std::cerr << "无法创建颜色空间转换上下文" << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }

    // *** 优化网络传输：增加缓冲区，保证流畅性 ***
    AVDictionary *io_opts = nullptr;
    av_dict_set(&io_opts, "tcp_nodelay", "1", 0);  // 禁用Nagle算法，立即发送数据包
    av_dict_set(&io_opts, "send_buffer_size", "1048576", 0);  // 设置发送缓冲区为1MB，增强抗网络抖动能力
    av_dict_set(&io_opts, "rw_timeout", "10000000", 0);  // 设置读写超时为10秒（微秒）
    av_dict_set(&io_opts, "buffer_size", "1048576", 0);  // 设置IO缓冲区为1MB
    
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        std::cout << "正在连接到RTMP服务器: " << rtmp_url << std::endl;
        
        // 使用网络选项打开连接
        ret = avio_open2(&fmt_ctx->pb, rtmp_url.c_str(), AVIO_FLAG_WRITE, nullptr, &io_opts);
        
        if (ret < 0) {
            std::cerr << "无法打开输出URL: " << avErrorToString(ret) << std::endl;
            std::cerr << "URL: " << rtmp_url << std::endl;
            
            // 提供详细的网络诊断信息
            std::cerr << "\n=== 网络诊断建议 ===" << std::endl;
            std::cerr << "1. 检查RTMP服务器是否运行: " << std::endl;
            std::cerr << "   命令: telnet 172.24.224.1 1935" << std::endl;
            std::cerr << "2. 检查防火墙是否开放1935端口" << std::endl;
            std::cerr << "3. 验证RTMP URL格式: rtmp://server:port/app/stream_key" << std::endl;
            std::cerr << "   当前URL可能缺少stream key，建议使用: rtmp://172.24.224.1:1935/live/stream" << std::endl;
            std::cerr << "4. 如果是Docker环境，检查网络模式和端口映射" << std::endl;
            std::cerr << "========================\n" << std::endl;
            
            av_dict_free(&io_opts);
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
            avcodec_free_context(&codec_ctx);
            avformat_free_context(fmt_ctx);
            fmt_ctx = nullptr;
            return false;
        }
        
        // 检查未使用的IO选项
        AVDictionaryEntry *entry = nullptr;
        while ((entry = av_dict_get(io_opts, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            std::cerr << "警告: 未使用的IO选项: " << entry->key << " = " << entry->value << std::endl;
        }
        av_dict_free(&io_opts);
        
        std::cout << "RTMP服务器连接成功（已启用低延迟模式）" << std::endl;
    }

    // 准备写入头部的选项（FLV格式选项）
    AVDictionary *format_opts = nullptr;
    av_dict_set(&format_opts, "flvflags", "no_duration_filesize", 0);  // 实时流不需要duration
    av_dict_set(&format_opts, "flush_packets", "1", 0);  // 立即刷新数据包，保证流畅性
    
    // 写入头部
    ret = avformat_write_header(fmt_ctx, &format_opts);
    
    // 检查未使用的选项
    AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(format_opts, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        std::cerr << "警告: 未使用的格式选项: " << entry->key << " = " << entry->value << std::endl;
    }
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
    
    // *** 验证FLV头部是否包含正确的H.264配置 ***
    std::cout << "FLV头部已写入，视频流extradata大小=" << video_stream->codecpar->extradata_size << " 字节" << std::endl;
    if (video_stream->codecpar->extradata_size == 0) {
        std::cerr << "错误: 视频流extradata为空，MediaMTX将无法解析H.264配置！" << std::endl;
    }

    // 重置帧计数和起始时间
    frame_count = 0;
    start_pts = AV_NOPTS_VALUE;
    last_dts = -1;
    i_frame_count = 0;
    p_frame_count = 0;
    
    std::cout << "RTMP推流初始化成功" << std::endl;
    std::cout << "  分辨率: " << width << "x" << height << std::endl;
    std::cout << "  帧率: " << fps << " fps" << std::endl;
    std::cout << "  比特率: " << (codec_ctx->bit_rate / 1000) << " kbps" << std::endl;
    std::cout << "  GOP大小: " << codec_ctx->gop_size << " 帧 (每" << (codec_ctx->gop_size / fps) << "秒一个关键帧)" << std::endl;
    std::cout << "  最小关键帧间隔: " << codec_ctx->keyint_min << " 帧" << std::endl;
    
    return true;
}

bool RTMPStreamer::pushFrame(const cv::Mat& frame) {
    if (!isInitialized()) {
        std::cerr << "RTMP推流未初始化" << std::endl;
        return false;
    }
    
    if (frame.empty()) {
        std::cerr << "RTMP推流: 帧为空" << std::endl;
        return false;
    }
    if (frame.type() != CV_8UC3) {
        std::cerr << "RTMP推流: 无效的视频帧格式" << std::endl;
        return false;
    }
    if (frame.cols != codec_ctx->width || frame.rows != codec_ctx->height) {
        std::cerr << "RTMP推流: 视频帧尺寸不匹配" << std::endl;
        return false;
    }
    
    AVFrame *av_frame = av_frame_alloc();
    if (!av_frame) {
        std::cerr << "无法分配AVFrame" << std::endl;
        return false;
    }
    
    av_frame->format = AV_PIX_FMT_YUV420P;
    av_frame->width = codec_ctx->width;
    av_frame->height = codec_ctx->height;
    
    // *** 低延迟优化：使用帧计数作为PTS，简化时间戳计算 ***
    av_frame->pts = frame_count;
    
    // *** 让编码器自动决定帧类型，不要手动干预 ***
    av_frame->pict_type = AV_PICTURE_TYPE_NONE;  // 让编码器根据GOP设置自动决定
    
    // 每10秒输出一次统计信息（减少日志输出）
    if (frame_count % 250 == 0) {
        std::cout << "RTMP推流运行中: 已推送 " << frame_count << " 帧 (I帧:" 
                  << i_frame_count << ", P帧:" << p_frame_count << ")" << std::endl;
    }
    
    frame_count++;
    
    int ret = av_frame_get_buffer(av_frame, 32);
    if (ret < 0) {
        std::cerr << "无法为AVFrame分配缓冲区: " << avErrorToString(ret) << std::endl;
        av_frame_free(&av_frame);
        return false;
    }
    
    const uint8_t* src_data[1] = { frame.data };
    int src_linesize[1] = { static_cast<int>(frame.step) };
    
    sws_scale(sws_ctx, src_data, src_linesize, 0, frame.rows,
              av_frame->data, av_frame->linesize);
    
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        std::cerr << "无法分配AVPacket" << std::endl;
        av_frame_free(&av_frame);
        return false;
    }
    
    ret = avcodec_send_frame(codec_ctx, av_frame);
    if (ret < 0) {
        std::cerr << "发送帧到编码器失败: " << avErrorToString(ret) << std::endl;
        av_frame_free(&av_frame);
        av_packet_free(&pkt);
        return false;
    }
    
    bool write_success = true;
    
    // *** 关键修复：循环接收所有编码后的包 ***
    int packet_count = 0;
    while (true) {
        // 重置packet以接收新数据
        av_packet_unref(pkt);
        
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN)) {
            // 编码器需要更多输入帧（正常情况）
            break;
        } else if (ret == AVERROR_EOF) {
            // 编码器已刷新完毕
            break;
        } else if (ret < 0) {
            std::cerr << "编码错误: " << avErrorToString(ret) << std::endl;
            write_success = false;
            break;
        }
        
        packet_count++;
        
        // 调试：输出编码器返回的原始时间戳
        int64_t original_pts = pkt->pts;
        int64_t original_dts = pkt->dts;
        
        // 统计帧类型
        bool is_key = (pkt->flags & AV_PKT_FLAG_KEY);
        if (is_key) {
            i_frame_count++;
        } else {
            p_frame_count++;
        }
        
        // 输出详细日志（仅前10帧用于调试）
        if (frame_count <= 10) {
            std::cout << "[帧 #" << (frame_count - 1) << "] 编码器返回包: 大小=" << pkt->size 
                      << ", PTS=" << original_pts << ", DTS=" << original_dts
                      << " (帧类型=" << (is_key ? "I帧(关键帧)" : "P帧") << ")" << std::endl;
        }
        
        // *** 关键修复：确保时间戳有效 ***
        if (pkt->pts == AV_NOPTS_VALUE) {
            pkt->pts = frame_count - 1;
            std::cerr << "警告: 编码器未设置PTS，手动设置为 " << pkt->pts << std::endl;
        }
        
        if (pkt->dts == AV_NOPTS_VALUE) {
            pkt->dts = pkt->pts;
            std::cerr << "警告: 编码器未设置DTS，使用PTS值 " << pkt->dts << std::endl;
        }
        
        // 保存缩放前的时间戳
        int64_t pts_before_rescale = pkt->pts;
        int64_t dts_before_rescale = pkt->dts;
        
        // 缩放时间戳：从编码器时间基 -> 流时间基
        av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
        pkt->stream_index = video_stream->index;
        
        // *** 关键检查：确保缩放后时间戳仍然有效 ***
        if (pkt->pts == AV_NOPTS_VALUE || pkt->dts == AV_NOPTS_VALUE) {
            std::cerr << "错误: 时间戳缩放后变为无效值！" << std::endl;
            std::cerr << "  缩放前 PTS=" << pts_before_rescale << ", DTS=" << dts_before_rescale << std::endl;
            std::cerr << "  编码器时间基: " << codec_ctx->time_base.num << "/" << codec_ctx->time_base.den << std::endl;
            std::cerr << "  流时间基: " << video_stream->time_base.num << "/" << video_stream->time_base.den << std::endl;
            
            // 手动计算缩放
            pkt->pts = av_rescale_q(pts_before_rescale, codec_ctx->time_base, video_stream->time_base);
            pkt->dts = av_rescale_q(dts_before_rescale, codec_ctx->time_base, video_stream->time_base);
            std::cerr << "  手动缩放后 PTS=" << pkt->pts << ", DTS=" << pkt->dts << std::endl;
        }
        
        // *** 低延迟优化：简化时间戳处理，减少计算开销 ***
        // 记录第一个包的PTS作为基准
        if (start_pts == AV_NOPTS_VALUE) {
            start_pts = pkt->pts;
            std::cout << "起始PTS: " << start_pts << " (低延迟模式)" << std::endl;
        }
        
        // 归零化时间戳（相对于起始时间）
        pkt->pts -= start_pts;
        pkt->dts -= start_pts;
        
        // 确保非负（避免时间戳回退）
        if (pkt->pts < 0) pkt->pts = 0;
        if (pkt->dts < 0) pkt->dts = 0;
        
        // 确保 DTS <= PTS（解码顺序不能晚于显示顺序）
        if (pkt->dts > pkt->pts) {
            pkt->dts = pkt->pts;
        }
        
        // *** 关键修复：确保DTS严格单调递增，避免花屏 ***
        if (last_dts >= 0 && pkt->dts <= last_dts) {
            // DTS必须严格递增，否则会导致解码器混乱
            pkt->dts = last_dts + 1;
            // 确保修正后 DTS 仍然 <= PTS
            if (pkt->dts > pkt->pts) {
                pkt->pts = pkt->dts;
            }
            std::cerr << "警告: DTS不单调，已修正为 " << pkt->dts << std::endl;
        }
        last_dts = pkt->dts;
        
        // *** 最终验证：确保写入前时间戳有效 ***
        if (pkt->pts == AV_NOPTS_VALUE || pkt->dts == AV_NOPTS_VALUE) {
            std::cerr << "致命错误: 写入前时间戳仍然无效！PTS=" << pkt->pts << ", DTS=" << pkt->dts << std::endl;
            write_success = false;
            break;
        }
        
        // *** 关键：在写入前保存PTS/DTS用于日志 ***
        int64_t pts_for_log = pkt->pts;
        int64_t dts_for_log = pkt->dts;
        int size_for_log = pkt->size;
        bool is_keyframe = (pkt->flags & AV_PKT_FLAG_KEY);
        
        // *** 使用av_interleaved_write_frame确保数据包正确排序，避免花屏 ***
        // 虽然延迟稍高，但能保证RTMP/FLV格式的数据包顺序正确
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        
        if (ret < 0) {
            std::cerr << "[帧 #" << (frame_count - 1) << "] 写入失败: " << avErrorToString(ret) 
                      << " (PTS: " << pts_for_log << ", DTS: " << dts_for_log << ", 大小: " << size_for_log << ")" << std::endl;
            write_success = false;
            break;
        }
        
        // 输出成功日志（仅前10帧用于调试）
        if (frame_count <= 10) {
            std::cout << "[帧 #" << (frame_count - 1) << "] 写入成功 (PTS=" << pts_for_log 
                      << ", DTS=" << dts_for_log << ", 大小=" << size_for_log << " 字节, "
                      << (is_keyframe ? "I帧" : "P帧") << ")" << std::endl;
        }
    }
    
    // *** 优化缓冲策略：每5帧刷新一次，保证实时性和流畅性的平衡 ***
    // av_interleaved_write_frame已经做了缓冲排序，这里定期刷新确保数据及时发送
    if (fmt_ctx && fmt_ctx->pb && (frame_count % 5 == 0)) {
        avio_flush(fmt_ctx->pb);
    }
    
    av_frame_free(&av_frame);
    av_packet_free(&pkt);
    
    // 只在失败时输出
    if (!write_success) {
        std::cerr << "[帧 #" << (frame_count - 1) << "] pushFrame 失败" << std::endl;
    }
    
    return write_success;
}

void RTMPStreamer::close() {
    if (codec_ctx) {
        avcodec_send_frame(codec_ctx, nullptr);
        
        AVPacket *pkt = av_packet_alloc();
        if (pkt) {
            while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
                if (fmt_ctx && video_stream) {
                    av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
                    pkt->stream_index = video_stream->index;
                    av_interleaved_write_frame(fmt_ctx, pkt);
                }
                av_packet_unref(pkt);
            }
            av_packet_free(&pkt);
        }
    }
    
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
    }
    
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
    
    // 输出最终统计
    if (frame_count > 0) {
        std::cout << "RTMP推流已关闭 - 总计推送 " << frame_count << " 帧 (I帧:" 
                  << i_frame_count << ", P帧:" << p_frame_count << ")" << std::endl;
    } else {
        std::cout << "RTMP推流已关闭" << std::endl;
    }
    
    frame_count = 0;
    start_pts = AV_NOPTS_VALUE;
    last_dts = -1;
    i_frame_count = 0;
    p_frame_count = 0;
    
    avformat_network_deinit();
}

} // namespace detector_service