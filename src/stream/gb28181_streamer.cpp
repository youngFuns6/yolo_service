#include "gb28181_streamer.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace detector_service {

/**
 * @brief 将FFmpeg错误码转换为字符串
 */
static std::string avErrorToString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

bool GB28181Streamer::initialize(const GB28181Config& config,
                                 int width, int height, int fps,
                                 const std::string& dest_ip, int dest_port,
                                 const std::string& ssrc,
                                 std::optional<int> bitrate) {
    std::lock_guard<std::mutex> lock(stream_mutex);
    
    // 保存配置
    this->dest_ip = dest_ip;
    this->dest_port = dest_port;
    this->ssrc = ssrc;
    this->is_ps_stream = (config.stream_mode == "PS");
    
    // 选择输出格式（PS流使用mpegts，H.264使用rtp_h264）
    const char* format_name = is_ps_stream ? "mpegts" : "rtp";
    
    // 构建输出URL
    std::ostringstream url_stream;
    url_stream << "rtp://" << dest_ip << ":" << dest_port;
    if (!ssrc.empty()) {
        url_stream << "?ssrc=" << ssrc;
    }
    std::string output_url = url_stream.str();
    
    // 分配输出格式上下文
    int ret = avformat_alloc_output_context2(&fmt_ctx, nullptr, format_name, output_url.c_str());
    if (ret < 0 || !fmt_ctx) {
        std::cerr << "GB28181: 无法创建输出格式上下文: " << avErrorToString(ret) << std::endl;
        return false;
    }
    
    // 查找H.264编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "GB28181: 未找到H.264编码器" << std::endl;
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 创建视频流
    video_stream = avformat_new_stream(fmt_ctx, codec);
    if (!video_stream) {
        std::cerr << "GB28181: 无法创建视频流" << std::endl;
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 分配编码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "GB28181: 无法分配编码器上下文" << std::endl;
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        return false;
    }
    
    // 设置编码参数
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = AVRational{1, fps};
    codec_ctx->framerate = AVRational{fps, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->gop_size = fps * 2;  // 2秒一个I帧
    codec_ctx->max_b_frames = 0;     // GB28181通常不使用B帧
    
    // 设置比特率
    if (bitrate.has_value() && bitrate.value() > 0) {
        codec_ctx->bit_rate = bitrate.value();
    } else {
        // 根据分辨率自动计算比特率
        codec_ctx->bit_rate = width * height * fps / 10;  // 简单估算
    }
    
    // H.264编码器特定选项
    av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(codec_ctx->priv_data, "profile", "baseline", 0);
    
    // 如果是PS流，需要设置一些额外参数
    if (is_ps_stream) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // 打开编码器
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "GB28181: 无法打开编码器: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        codec_ctx = nullptr;
        return false;
    }
    
    // 将编码器参数复制到流
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0) {
        std::cerr << "GB28181: 无法复制编码器参数: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        codec_ctx = nullptr;
        return false;
    }
    
    video_stream->time_base = codec_ctx->time_base;
    
    // 打开输出
    ret = avio_open(&fmt_ctx->pb, output_url.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        std::cerr << "GB28181: 无法打开输出URL " << output_url << ": " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        codec_ctx = nullptr;
        return false;
    }
    
    // 写入文件头
    ret = avformat_write_header(fmt_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "GB28181: 无法写入文件头: " << avErrorToString(ret) << std::endl;
        avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        codec_ctx = nullptr;
        return false;
    }
    
    // 初始化图像转换上下文（BGR to YUV420P）
    sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        std::cerr << "GB28181: 无法创建图像转换上下文" << std::endl;
        avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        codec_ctx = nullptr;
        return false;
    }
    
    frame_count = 0;
    start_pts = AV_NOPTS_VALUE;
    last_dts = -1;
    is_streaming = true;
    
    return true;
}

bool GB28181Streamer::pushFrame(const cv::Mat& frame) {
    if (!isInitialized() || !is_streaming) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(stream_mutex);
    
    // 分配AVFrame
    AVFrame* av_frame = av_frame_alloc();
    if (!av_frame) {
        std::cerr << "GB28181: 无法分配AVFrame" << std::endl;
        return false;
    }
    
    av_frame->format = codec_ctx->pix_fmt;
    av_frame->width = codec_ctx->width;
    av_frame->height = codec_ctx->height;
    
    int ret = av_frame_get_buffer(av_frame, 0);
    if (ret < 0) {
        std::cerr << "GB28181: 无法分配帧缓冲区: " << avErrorToString(ret) << std::endl;
        av_frame_free(&av_frame);
        return false;
    }
    
    // 转换图像格式 BGR -> YUV420P
    const uint8_t* src_data[1] = { frame.data };
    int src_linesize[1] = { static_cast<int>(frame.step[0]) };
    
    sws_scale(sws_ctx, src_data, src_linesize, 0, frame.rows,
              av_frame->data, av_frame->linesize);
    
    // 设置PTS
    av_frame->pts = frame_count++;
    
    // 发送帧到编码器
    ret = avcodec_send_frame(codec_ctx, av_frame);
    av_frame_free(&av_frame);
    
    if (ret < 0) {
        std::cerr << "GB28181: 发送帧到编码器失败: " << avErrorToString(ret) << std::endl;
        return false;
    }
    
    // 接收编码后的数据包
    bool success = true;
    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            success = false;
            break;
        }
        
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            std::cerr << "GB28181: 接收数据包失败: " << avErrorToString(ret) << std::endl;
            av_packet_free(&pkt);
            success = false;
            break;
        }
        
        // 设置时间戳
        pkt->stream_index = video_stream->index;
        av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
        
        // 确保DTS单调递增
        if (last_dts >= 0 && pkt->dts <= last_dts) {
            pkt->dts = last_dts + 1;
            if (pkt->pts < pkt->dts) {
                pkt->pts = pkt->dts;
            }
        }
        last_dts = pkt->dts;
        
        // 写入数据包
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_free(&pkt);
        
        if (ret < 0) {
            std::cerr << "GB28181: 写入数据包失败: " << avErrorToString(ret) << std::endl;
            success = false;
            break;
        }
    }
    
    return success;
}

void GB28181Streamer::close() {
    std::lock_guard<std::mutex> lock(stream_mutex);
    
    if (!isInitialized()) {
        return;
    }
    
    is_streaming = false;
    
    // 刷新编码器
    if (codec_ctx) {
        avcodec_send_frame(codec_ctx, nullptr);
        
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    
    // 写入文件尾
    if (fmt_ctx && fmt_ctx->pb) {
        av_write_trailer(fmt_ctx);
    }
    
    // 清理资源
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
        codec_ctx = nullptr;
    }
    
    if (fmt_ctx) {
        if (fmt_ctx->pb) {
            avio_closep(&fmt_ctx->pb);
        }
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
    }
}

} // namespace detector_service

