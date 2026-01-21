#ifdef ENABLE_BM1684

#include "bm_hw_codec.h"
#include <iostream>
#include <cstring>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#define STEP_ALIGNMENT 32

namespace detector_service {

// ============================================================================
// BMVideoDecoder 实现
// ============================================================================

BMVideoDecoder::BMVideoDecoder()
    : ifmt_ctx_(nullptr)
    , video_dec_ctx_(nullptr)
    , video_dec_par_(nullptr)
    , decoder_(nullptr)
    , frame_(nullptr)
    , width_(0)
    , height_(0)
    , pix_fmt_(0)
    , video_stream_idx_(-1)
    , refcount_(1)
{
    av_init_packet(&pkt_);
    pkt_.data = nullptr;
    pkt_.size = 0;
    frame_ = av_frame_alloc();
}

BMVideoDecoder::~BMVideoDecoder() {
    closeDec();
}

bool BMVideoDecoder::openDec(const std::string& filename,
                            const std::string& codec_name,
                            int output_format_mode,
                            int extra_frame_buffer_num,
                            int sophon_idx,
                            int pcie_no_copyback) {
    int ret = 0;
    AVDictionary *dict = nullptr;
    av_dict_set(&dict, "rtsp_flags", "prefer_tcp", 0);
    
    ret = avformat_open_input(&ifmt_ctx_, filename.c_str(), nullptr, &dict);
    if (ret < 0) {
        std::cerr << "BMVideoDecoder: Cannot open input: " << filename << std::endl;
        av_dict_free(&dict);
        return false;
    }

    ret = avformat_find_stream_info(ifmt_ctx_, nullptr);
    if (ret < 0) {
        std::cerr << "BMVideoDecoder: Cannot find stream information" << std::endl;
        av_dict_free(&dict);
        return false;
    }

    int codec_name_flag = codec_name.empty() ? 0 : 1;
    const char* coder_name = codec_name.empty() ? nullptr : codec_name.c_str();
    
#ifdef BM_PCIE_MODE
    ret = openCodecContext(&video_stream_idx_, &video_dec_ctx_, ifmt_ctx_, AVMEDIA_TYPE_VIDEO,
                          codec_name_flag, coder_name, output_format_mode, extra_frame_buffer_num,
                          sophon_idx, pcie_no_copyback);
#else
    ret = openCodecContext(&video_stream_idx_, &video_dec_ctx_, ifmt_ctx_, AVMEDIA_TYPE_VIDEO,
                          codec_name_flag, coder_name, output_format_mode, extra_frame_buffer_num);
#endif
    
    if (ret >= 0) {
        width_ = video_dec_ctx_->width;
        height_ = video_dec_ctx_->height;
        pix_fmt_ = video_dec_ctx_->pix_fmt;
    }
    
    av_dict_free(&dict);
    return ret >= 0;
}

void BMVideoDecoder::closeDec() {
    if (video_dec_ctx_) {
        avcodec_free_context(&video_dec_ctx_);
        video_dec_ctx_ = nullptr;
    }
    if (ifmt_ctx_) {
        avformat_close_input(&ifmt_ctx_);
        ifmt_ctx_ = nullptr;
    }
    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
}

AVCodec* BMVideoDecoder::findBmDecoder(AVCodecID dec_id, const std::string& name,
                                       int codec_name_flag, enum AVMediaType type) {
    AVCodec *codec = nullptr;
    
    if (codec_name_flag && type == AVMEDIA_TYPE_VIDEO) {
        const AVCodecDescriptor *desc;
        const char *codec_string = "decoder";

        codec = avcodec_find_decoder_by_name(name.c_str());
        if (!codec && (desc = avcodec_descriptor_get_by_name(name.c_str()))) {
            codec = avcodec_find_decoder(desc->id);
        }

        if (!codec) {
            std::cerr << "BMVideoDecoder: Unknown decoder '" << name << "'" << std::endl;
            return nullptr;
        }
        if (codec->type != type) {
            std::cerr << "BMVideoDecoder: Invalid decoder type '" << name << "'" << std::endl;
            return nullptr;
        }
    } else {
        codec = avcodec_find_decoder(dec_id);
    }

    if (!codec) {
        std::cerr << "BMVideoDecoder: Failed to find codec" << std::endl;
        return nullptr;
    }
    
    return codec;
}

int BMVideoDecoder::openCodecContext(int *stream_idx, AVCodecContext **dec_ctx,
                                    AVFormatContext *fmt_ctx, enum AVMediaType type,
                                    int codec_name_flag, const std::string& coder_name,
                                    int output_format_mode, int extra_frame_buffer_num,
                                    int sophon_idx, int pcie_no_copyback) {
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = nullptr;
    AVDictionary *opts = nullptr;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
    if (ret < 0) {
        std::cerr << "BMVideoDecoder: Could not find video stream" << std::endl;
        return ret;
    }

    stream_index = ret;
    st = fmt_ctx->streams[stream_index];

    // 查找解码器
    if (codec_name_flag && !coder_name.empty()) {
        decoder_ = findBmDecoder((AVCodecID)0, coder_name, codec_name_flag, AVMEDIA_TYPE_VIDEO);
    } else {
        decoder_ = findBmDecoder(st->codecpar->codec_id);
    }
    
    if (!decoder_) {
        std::cerr << "BMVideoDecoder: Failed to find decoder" << std::endl;
        return AVERROR(EINVAL);
    }

    // 分配解码器上下文
    *dec_ctx = avcodec_alloc_context3(decoder_);
    if (!*dec_ctx) {
        std::cerr << "BMVideoDecoder: Failed to allocate codec context" << std::endl;
        return AVERROR(ENOMEM);
    }

    // 复制编解码参数
    ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar);
    if (ret < 0) {
        std::cerr << "BMVideoDecoder: Failed to copy codec parameters" << std::endl;
        return ret;
    }

    video_dec_par_ = st->codecpar;
    
    // 设置解码器选项
    av_dict_set(&opts, "refcounted_frames", refcount_ ? "1" : "0", 0);
#ifdef BM_PCIE_MODE
    av_dict_set_int(&opts, "zero_copy", pcie_no_copyback, 0);
    av_dict_set_int(&opts, "sophon_idx", sophon_idx, 0);
#endif
    if (output_format_mode == 101) {
        av_dict_set_int(&opts, "output_format", output_format_mode, 18);
    }
    av_dict_set_int(&opts, "extra_frame_buffer_num", extra_frame_buffer_num, 0);

    ret = avcodec_open2(*dec_ctx, decoder_, &opts);
    if (ret < 0) {
        std::cerr << "BMVideoDecoder: Failed to open codec" << std::endl;
        av_dict_free(&opts);
        return ret;
    }
    
    *stream_idx = stream_index;
    av_dict_free(&opts);
    return 0;
}

AVFrame* BMVideoDecoder::grabFrame() {
    int ret = 0;
    int got_frame = 0;
    
#ifndef WIN32
    struct timeval tv1, tv2;
    gettimeofday(&tv1, nullptr);
#endif

    while (1) {
        av_packet_unref(&pkt_);
        ret = av_read_frame(ifmt_ctx_, &pkt_);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
#ifndef WIN32
                gettimeofday(&tv2, nullptr);
                if (((tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000) > 1000 * 60) {
                    std::cerr << "BMVideoDecoder: av_read_frame failed, retry time >60s" << std::endl;
                    break;
                }
                usleep(10 * 1000);
#endif
                continue;
            }
            return nullptr;
        }

        if (pkt_.stream_index != video_stream_idx_) {
            continue;
        }

        if (!frame_) {
            std::cerr << "BMVideoDecoder: Could not allocate frame" << std::endl;
            return nullptr;
        }

        if (refcount_) {
            av_frame_unref(frame_);
        }

        ret = avcodec_decode_video2(video_dec_ctx_, frame_, &got_frame, &pkt_);
        if (ret < 0) {
            std::cerr << "BMVideoDecoder: Error decoding video frame" << std::endl;
            continue;
        }

        if (!got_frame) {
            continue;
        }

        width_ = video_dec_ctx_->width;
        height_ = video_dec_ctx_->height;
        pix_fmt_ = video_dec_ctx_->pix_fmt;
        
        if (frame_->width != width_ || frame_->height != height_ || frame_->format != pix_fmt_) {
            std::cerr << "BMVideoDecoder: Video format changed" << std::endl;
            continue;
        }

        break;
    }
    
    return frame_;
}

// ============================================================================
// BMVideoEncoder 实现
// ============================================================================

BMVideoEncoder::BMVideoEncoder()
    : ofmt_ctx_(nullptr)
    , enc_ctx_(nullptr)
    , picture_(nullptr)
    , input_picture_(nullptr)
    , out_stream_(nullptr)
    , aligned_input_(nullptr)
    , frame_width_(0)
    , frame_height_(0)
    , frame_idx_(0)
{
}

BMVideoEncoder::~BMVideoEncoder() {
    closeEnc();
}

AVCodec* BMVideoEncoder::findHwVideoEncoder(int codecId) {
    AVCodec *encoder = nullptr;
    switch (codecId) {
        case AV_CODEC_ID_H264:
            encoder = avcodec_find_encoder_by_name("h264_bm");
            break;
        case AV_CODEC_ID_H265:
            encoder = avcodec_find_encoder_by_name("h265_bm");
            break;
        default:
            break;
    }
    return encoder;
}

bool BMVideoEncoder::openEnc(const std::string& filename,
                            int soc_idx,
                            int codec_id,
                            int framerate,
                            int width,
                            int height,
                            int inputformat,
                            int bitrate,
                            int roi_enable) {
    int ret = 0;
    AVCodec *encoder;
    AVDictionary *dict = nullptr;
    frame_idx_ = 0;
    frame_width_ = width;
    frame_height_ = height;

    ret = avformat_alloc_output_context2(&ofmt_ctx_, nullptr, nullptr, filename.c_str());
    if (!ofmt_ctx_) {
        std::cerr << "BMVideoEncoder: Could not create output context" << std::endl;
        return false;
    }

    encoder = findHwVideoEncoder(codec_id);
    if (!encoder) {
        std::cerr << "BMVideoEncoder: Hardware video encoder not found" << std::endl;
        return false;
    }

    enc_ctx_ = avcodec_alloc_context3(encoder);
    if (!enc_ctx_) {
        std::cerr << "BMVideoEncoder: Failed to allocate encoder context" << std::endl;
        return false;
    }
    
    enc_ctx_->codec_id = (AVCodecID)codec_id;
    enc_ctx_->width = width;
    enc_ctx_->height = height;
    enc_ctx_->pix_fmt = (AVPixelFormat)inputformat;
    enc_ctx_->bit_rate_tolerance = bitrate;
    enc_ctx_->bit_rate = (int64_t)bitrate;
    enc_ctx_->gop_size = 32;
    enc_ctx_->time_base.num = 1;
    enc_ctx_->time_base.den = framerate;
    enc_ctx_->framerate.num = framerate;
    enc_ctx_->framerate.den = 1;

    out_stream_ = avformat_new_stream(ofmt_ctx_, encoder);
    out_stream_->time_base = enc_ctx_->time_base;
    out_stream_->avg_frame_rate = enc_ctx_->framerate;
    out_stream_->r_frame_rate = out_stream_->avg_frame_rate;
    
    av_dict_set_int(&dict, "sophon_idx", soc_idx, 0);
    av_dict_set_int(&dict, "gop_preset", 8, 0);
    av_dict_set_int(&dict, "is_dma_buffer", 0, 0);
    av_dict_set_int(&dict, "roi_enable", roi_enable, 0);

    ret = avcodec_open2(enc_ctx_, encoder, &dict);
    if (ret < 0) {
        std::cerr << "BMVideoEncoder: Cannot open video encoder" << std::endl;
        av_dict_free(&dict);
        return false;
    }
    
    ret = avcodec_parameters_from_context(out_stream_->codecpar, enc_ctx_);
    if (ret < 0) {
        std::cerr << "BMVideoEncoder: Failed to copy encoder parameters" << std::endl;
        av_dict_free(&dict);
        return false;
    }
    
    if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx_->pb, filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "BMVideoEncoder: Could not open output file" << std::endl;
            av_dict_free(&dict);
            return false;
        }
    }
    
    ret = avformat_write_header(ofmt_ctx_, nullptr);
    if (ret < 0) {
        std::cerr << "BMVideoEncoder: Error occurred when opening output file" << std::endl;
        av_dict_free(&dict);
        return false;
    }
    
    av_dict_free(&dict);

    picture_ = av_frame_alloc();
    picture_->format = enc_ctx_->pix_fmt;
    picture_->width = width;
    picture_->height = height;

    return true;
}

int BMVideoEncoder::writeFrame(const uint8_t* data, int step, int width, int height) {
    int ret = 0;
    int got_output = 0;
    
    if (step % STEP_ALIGNMENT != 0) {
        std::cerr << "BMVideoEncoder: input step must align with " << STEP_ALIGNMENT << std::endl;
        return -1;
    }

    av_image_fill_arrays(picture_->data, picture_->linesize, data, enc_ctx_->pix_fmt, width, height, 1);
    picture_->linesize[0] = step;
    picture_->pts = frame_idx_;
    frame_idx_++;

    AVPacket enc_pkt;
    enc_pkt.data = nullptr;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    
    ret = avcodec_encode_video2(enc_ctx_, &enc_pkt, picture_, &got_output);
    if (ret < 0) {
        return ret;
    }
    
    if (got_output == 0) {
        std::cerr << "BMVideoEncoder: No output from encoder" << std::endl;
        return -1;
    }

    av_packet_rescale_ts(&enc_pkt, enc_ctx_->time_base, out_stream_->time_base);
    ret = av_interleaved_write_frame(ofmt_ctx_, &enc_pkt);
    
    return ret;
}

int BMVideoEncoder::flushEncoder() {
    int ret;
    int got_frame = 0;

    if (!(enc_ctx_->codec->capabilities & AV_CODEC_CAP_DELAY)) {
        return 0;
    }

    while (1) {
        AVPacket enc_pkt;
        enc_pkt.data = nullptr;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);

        ret = avcodec_encode_video2(enc_ctx_, &enc_pkt, nullptr, &got_frame);
        if (ret < 0) {
            return ret;
        }

        if (!got_frame) {
            break;
        }

        av_packet_rescale_ts(&enc_pkt, enc_ctx_->time_base, out_stream_->time_base);
        ret = av_interleaved_write_frame(ofmt_ctx_, &enc_pkt);
        if (ret < 0) {
            break;
        }
    }

    return ret;
}

void BMVideoEncoder::closeEnc() {
    flushEncoder();
    av_write_trailer(ofmt_ctx_);

    if (picture_) {
        av_frame_free(&picture_);
        picture_ = nullptr;
    }

    if (input_picture_) {
        av_free(input_picture_);
        input_picture_ = nullptr;
    }

    if (enc_ctx_) {
        avcodec_free_context(&enc_ctx_);
        enc_ctx_ = nullptr;
    }

    if (ofmt_ctx_ && !(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx_->pb);
    }
    
    if (ofmt_ctx_) {
        avformat_free_context(&ofmt_ctx_);
        ofmt_ctx_ = nullptr;
    }
}

} // namespace detector_service

#endif // ENABLE_BM1684
