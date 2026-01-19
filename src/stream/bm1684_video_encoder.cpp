#ifdef ENABLE_BM1684

#include "bm1684_video_encoder.h"
#include <iostream>
#include <cstring>

extern "C" {
#include <libswscale/swscale.h>
}

namespace detector_service {

BM1684VideoEncoder::BM1684VideoEncoder()
    : enc_ctx_(nullptr),
      encoder_(nullptr),
      frame_(nullptr),
      sws_ctx_(nullptr),
      width_(0),
      height_(0),
      fps_(0),
      frame_count_(0) {
}

BM1684VideoEncoder::~BM1684VideoEncoder() {
    close();
}

AVCodec* BM1684VideoEncoder::findBmEncoder(const std::string& codec_name) {
    AVCodec* encoder = nullptr;
    
    if (!codec_name.empty()) {
        // 查找BM1684硬件编码器
        encoder = avcodec_find_encoder_by_name(codec_name.c_str());
        if (!encoder) {
            const AVCodecDescriptor* desc = avcodec_descriptor_get_by_name(codec_name.c_str());
            if (desc) {
                encoder = avcodec_find_encoder(desc->id);
            }
        }
        
        if (encoder && encoder->type != AVMEDIA_TYPE_VIDEO) {
            std::cerr << "BM1684: 编码器 '" << codec_name << "' 不是视频编码器" << std::endl;
            return nullptr;
        }
    }
    
    if (!encoder) {
        // 回退到标准编码器
        if (codec_name.find("h264") != std::string::npos) {
            encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        } else if (codec_name.find("h265") != std::string::npos || 
                   codec_name.find("hevc") != std::string::npos) {
            encoder = avcodec_find_encoder(AV_CODEC_ID_H265);
        }
    }
    
    return encoder;
}

bool BM1684VideoEncoder::initialize(const std::string& codec_name,
                                   int width, int height, int fps,
                                   int bitrate, int gop_size,
                                   int sophon_idx,
                                   bool use_dma_buffer) {
    if (isInitialized()) {
        close();
    }
    
    width_ = width;
    height_ = height;
    fps_ = fps;
    frame_count_ = 0;
    
    // 查找BM1684硬件编码器
    encoder_ = findBmEncoder(codec_name);
    if (!encoder_) {
        std::cerr << "BM1684: 无法找到编码器: " << codec_name << std::endl;
        return false;
    }
    
    // 分配编码器上下文
    enc_ctx_ = avcodec_alloc_context3(encoder_);
    if (!enc_ctx_) {
        std::cerr << "BM1684: 无法分配编码器上下文" << std::endl;
        return false;
    }
    
    // 设置编码参数
    enc_ctx_->codec_id = encoder_->id;
    enc_ctx_->width = width;
    enc_ctx_->height = height;
    enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx_->bit_rate = bitrate;
    enc_ctx_->bit_rate_tolerance = bitrate;
    enc_ctx_->gop_size = gop_size;
    enc_ctx_->max_b_frames = 0;  // GB28181通常不使用B帧
    enc_ctx_->time_base.num = 1;
    enc_ctx_->time_base.den = fps;
    enc_ctx_->framerate.num = fps;
    enc_ctx_->framerate.den = 1;
    
    // 设置BM1684特定选项
    AVDictionary* opts = nullptr;
    av_dict_set_int(&opts, "sophon_idx", sophon_idx, 0);
    av_dict_set_int(&opts, "gop_preset", gop_size, 0);
    av_dict_set_int(&opts, "is_dma_buffer", use_dma_buffer ? 1 : 0, 0);
    
    // H.264/H.265特定选项
    if (encoder_->id == AV_CODEC_ID_H264) {
        av_opt_set(enc_ctx_->priv_data, "profile", "baseline", 0);
        av_opt_set(enc_ctx_->priv_data, "preset", "ultrafast", 0);
        av_opt_set(enc_ctx_->priv_data, "tune", "zerolatency", 0);
    }
    
    // 打开编码器
    int ret = avcodec_open2(enc_ctx_, encoder_, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "BM1684: 无法打开编码器: " << errbuf << std::endl;
        avcodec_free_context(&enc_ctx_);
        return false;
    }
    
    // 分配AVFrame
    frame_ = av_frame_alloc();
    if (!frame_) {
        std::cerr << "BM1684: 无法分配AVFrame" << std::endl;
        avcodec_free_context(&enc_ctx_);
        return false;
    }
    
    frame_->format = enc_ctx_->pix_fmt;
    frame_->width = width;
    frame_->height = height;
    
    ret = av_frame_get_buffer(frame_, 0);
    if (ret < 0) {
        std::cerr << "BM1684: 无法分配帧缓冲区" << std::endl;
        av_frame_free(&frame_);
        avcodec_free_context(&enc_ctx_);
        return false;
    }
    
    // 初始化图像转换上下文（BGR -> YUV420P）
    sws_ctx_ = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx_) {
        std::cerr << "BM1684: 无法创建图像转换上下文" << std::endl;
        av_frame_free(&frame_);
        avcodec_free_context(&enc_ctx_);
        return false;
    }
    
    std::cout << "BM1684: 成功初始化硬件编码器: " << codec_name 
              << ", 分辨率: " << width << "x" << height 
              << ", 帧率: " << fps << ", 比特率: " << bitrate << std::endl;
    
    return true;
}

bool BM1684VideoEncoder::convertMatToFrame(const cv::Mat& mat, AVFrame* av_frame) {
    if (!sws_ctx_ || !av_frame) {
        return false;
    }
    
    // 确保输入图像尺寸匹配
    if (mat.cols != width_ || mat.rows != height_) {
        std::cerr << "BM1684: 输入图像尺寸不匹配: " 
                  << mat.cols << "x" << mat.rows 
                  << " vs " << width_ << "x" << height_ << std::endl;
        return false;
    }
    
    // 转换BGR到YUV420P
    const uint8_t* src_data[1] = { mat.data };
    int src_linesize[1] = { static_cast<int>(mat.step[0]) };
    
    sws_scale(sws_ctx_, src_data, src_linesize, 0, height_,
              av_frame->data, av_frame->linesize);
    
    return true;
}

bool BM1684VideoEncoder::encodeFrame(const cv::Mat& frame) {
    if (!isInitialized() || !frame_) {
        return false;
    }
    
    // 转换Mat到AVFrame
    if (!convertMatToFrame(frame, frame_)) {
        return false;
    }
    
    // 设置PTS
    frame_->pts = frame_count_++;
    
    // 发送帧到编码器
    int ret = avcodec_send_frame(enc_ctx_, frame_);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "BM1684: 发送帧到编码器失败: " << errbuf << std::endl;
        return false;
    }
    
    return true;
}

bool BM1684VideoEncoder::getEncodedPacket(AVPacket* pkt) {
    if (!isInitialized() || !pkt) {
        return false;
    }
    
    int ret = avcodec_receive_packet(enc_ctx_, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return false;  // 需要更多输入或已结束
    } else if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "BM1684: 接收数据包失败: " << errbuf << std::endl;
        return false;
    }
    
    return true;
}

bool BM1684VideoEncoder::flush() {
    if (!isInitialized()) {
        return false;
    }
    
    // 发送NULL帧以刷新编码器
    int ret = avcodec_send_frame(enc_ctx_, nullptr);
    if (ret < 0) {
        return false;
    }
    
    return true;
}

void BM1684VideoEncoder::close() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
    
    if (enc_ctx_) {
        avcodec_free_context(&enc_ctx_);
        enc_ctx_ = nullptr;
    }
    
    encoder_ = nullptr;
    width_ = 0;
    height_ = 0;
    fps_ = 0;
    frame_count_ = 0;
}

} // namespace detector_service

#endif // ENABLE_BM1684

