#ifdef ENABLE_BM1684

#include "bm1684_video_decoder.h"
#include <iostream>
#include <cstring>

namespace detector_service {

BM1684VideoDecoder::BM1684VideoDecoder()
    : ifmt_ctx_(nullptr),
      video_dec_ctx_(nullptr),
      video_dec_par_(nullptr),
      decoder_(nullptr),
      width_(0),
      height_(0),
      pix_fmt_(0),
      video_stream_idx_(-1),
      frame_(nullptr) {
    av_init_packet(&pkt_);
    pkt_.data = nullptr;
    pkt_.size = 0;
    frame_ = av_frame_alloc();
}

BM1684VideoDecoder::~BM1684VideoDecoder() {
    close();
    if (frame_) {
        av_frame_free(&frame_);
    }
}

AVCodec* BM1684VideoDecoder::findBmDecoder(AVCodecID dec_id, const std::string& name) {
    AVCodec* codec = nullptr;
    
    if (!name.empty()) {
        // 尝试按名称查找BM1684硬件解码器
        codec = avcodec_find_decoder_by_name(name.c_str());
        if (!codec) {
            const AVCodecDescriptor* desc = avcodec_descriptor_get_by_name(name.c_str());
            if (desc) {
                codec = avcodec_find_decoder(desc->id);
            }
        }
        
        if (codec && codec->type != AVMEDIA_TYPE_VIDEO) {
            std::cerr << "BM1684: 解码器 '" << name << "' 不是视频解码器" << std::endl;
            return nullptr;
        }
    }
    
    if (!codec) {
        // 回退到标准解码器
        codec = avcodec_find_decoder(dec_id);
    }
    
    return codec;
}

int BM1684VideoDecoder::openCodecContext(int* stream_idx,
                                        AVCodecContext** dec_ctx,
                                        AVFormatContext* fmt_ctx,
                                        const std::string& codec_name,
                                        int sophon_idx,
                                        int pcie_no_copyback) {
    int ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        std::cerr << "BM1684: 无法找到视频流" << std::endl;
        return ret;
    }
    
    int stream_index = ret;
    AVStream* st = fmt_ctx->streams[stream_index];
    
    // 查找BM1684硬件解码器
    decoder_ = findBmDecoder(st->codecpar->codec_id, codec_name);
    if (!decoder_) {
        std::cerr << "BM1684: 无法找到解码器" << std::endl;
        return AVERROR(EINVAL);
    }
    
    // 分配解码器上下文
    *dec_ctx = avcodec_alloc_context3(decoder_);
    if (!*dec_ctx) {
        std::cerr << "BM1684: 无法分配解码器上下文" << std::endl;
        return AVERROR(ENOMEM);
    }
    
    // 复制编解码参数
    ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar);
    if (ret < 0) {
        std::cerr << "BM1684: 无法复制编解码参数" << std::endl;
        return ret;
    }
    
    video_dec_par_ = st->codecpar;
    
    // 设置BM1684特定选项
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "refcounted_frames", "1", 0);
    
#ifdef BM_PCIE_MODE
    av_dict_set_int(&opts, "zero_copy", pcie_no_copyback, 0);
    av_dict_set_int(&opts, "sophon_idx", sophon_idx, 0);
#endif
    
    // 设置额外帧缓冲区数量（用于DMA buffer模式）
    av_dict_set_int(&opts, "extra_frame_buffer_num", 5, 0);
    
    // 打开解码器
    ret = avcodec_open2(*dec_ctx, decoder_, &opts);
    if (ret < 0) {
        std::cerr << "BM1684: 无法打开解码器" << std::endl;
        av_dict_free(&opts);
        return ret;
    }
    
    *stream_idx = stream_index;
    av_dict_free(&opts);
    
    return 0;
}

bool BM1684VideoDecoder::open(const std::string& url,
                              const std::string& codec_name,
                              int sophon_idx,
                              int pcie_no_copyback) {
    int ret = 0;
    AVDictionary* dict = nullptr;
    
    // 设置RTSP选项
    av_dict_set(&dict, "rtsp_flags", "prefer_tcp", 0);
    av_dict_set(&dict, "stimeout", "5000000", 0);  // 5秒超时
    
    // 打开输入流
    ret = avformat_open_input(&ifmt_ctx_, url.c_str(), nullptr, &dict);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "BM1684: 无法打开输入流: " << errbuf << std::endl;
        av_dict_free(&dict);
        return false;
    }
    
    // 查找流信息
    ret = avformat_find_stream_info(ifmt_ctx_, nullptr);
    if (ret < 0) {
        std::cerr << "BM1684: 无法查找流信息" << std::endl;
        av_dict_free(&dict);
        return false;
    }
    
    av_dict_free(&dict);
    
    // 打开视频解码器
    ret = openCodecContext(&video_stream_idx_, &video_dec_ctx_, ifmt_ctx_,
                         codec_name, sophon_idx, pcie_no_copyback);
    if (ret < 0) {
        return false;
    }
    
    width_ = video_dec_ctx_->width;
    height_ = video_dec_ctx_->height;
    pix_fmt_ = video_dec_ctx_->pix_fmt;
    
    std::cout << "BM1684: 成功打开视频流: " << url << std::endl;
    std::cout << "BM1684: 分辨率: " << width_ << "x" << height_ 
              << ", 像素格式: " << pix_fmt_ << std::endl;
    
    return true;
}

void BM1684VideoDecoder::close() {
    if (video_dec_ctx_) {
        avcodec_free_context(&video_dec_ctx_);
        video_dec_ctx_ = nullptr;
    }
    
    if (ifmt_ctx_) {
        avformat_close_input(&ifmt_ctx_);
        ifmt_ctx_ = nullptr;
    }
    
    video_stream_idx_ = -1;
    width_ = 0;
    height_ = 0;
    pix_fmt_ = 0;
}

bool BM1684VideoDecoder::avFrameToMat(AVFrame* av_frame, cv::Mat& mat) {
    if (!av_frame || !av_frame->data[0]) {
        return false;
    }
    
    // 根据像素格式转换
    int cv_type = CV_8UC3;
    int cv_format = CV_BGR;
    
    switch (av_frame->format) {
        case AV_PIX_FMT_RGB24:
            cv_format = CV_RGB;
            break;
        case AV_PIX_FMT_BGR24:
            cv_format = CV_BGR;
            break;
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
            // 需要转换为RGB/BGR
            // 这里简化处理，实际应该使用sws_scale或bmcv转换
            std::cerr << "BM1684: 需要转换YUV格式" << std::endl;
            return false;
        default:
            std::cerr << "BM1684: 不支持的像素格式: " << av_frame->format << std::endl;
            return false;
    }
    
    // 创建Mat（直接使用AVFrame的数据，避免拷贝）
    mat = cv::Mat(av_frame->height, av_frame->width, cv_type, av_frame->data[0], av_frame->linesize[0]);
    
    // 如果需要转换颜色空间
    if (cv_format == CV_RGB) {
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    }
    
    // 复制数据（因为AVFrame可能被释放）
    mat = mat.clone();
    
    return true;
}

bool BM1684VideoDecoder::read(cv::Mat& frame) {
    if (!video_dec_ctx_ || !ifmt_ctx_) {
        return false;
    }
    
    int ret = 0;
    int got_frame = 0;
    
    while (true) {
        av_packet_unref(&pkt_);
        
        // 读取数据包
        ret = av_read_frame(ifmt_ctx_, &pkt_);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                // 重试
                continue;
            }
            // EOF或其他错误
            return false;
        }
        
        // 只处理视频流
        if (pkt_.stream_index != video_stream_idx_) {
            continue;
        }
        
        // 解码帧
        av_frame_unref(frame_);
        ret = avcodec_decode_video2(video_dec_ctx_, frame_, &got_frame, &pkt_);
        if (ret < 0) {
            std::cerr << "BM1684: 解码视频帧失败" << std::endl;
            continue;
        }
        
        if (!got_frame) {
            continue;
        }
        
        // 转换为OpenCV Mat
        if (avFrameToMat(frame_, frame)) {
            return true;
        }
    }
    
    return false;
}

int BM1684VideoDecoder::getFPS() const {
    if (!ifmt_ctx_ || video_stream_idx_ < 0) {
        return 0;
    }
    
    AVStream* stream = ifmt_ctx_->streams[video_stream_idx_];
    AVRational fps = stream->r_frame_rate;
    
    if (fps.den > 0) {
        return static_cast<int>((fps.num + fps.den / 2) / fps.den);
    }
    
    return 0;
}

} // namespace detector_service

#endif // ENABLE_BM1684

