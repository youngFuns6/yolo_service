#pragma once

#ifdef ENABLE_BM1684

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <optional>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace detector_service {

class BM1684VideoEncoder {
public:
    BM1684VideoEncoder();
    ~BM1684VideoEncoder();
    
    // 初始化编码器
    // codec_name: "h264_bm" 或 "h265_bm"
    // sophon_idx: BM1684设备索引
    // bitrate: 比特率（bps）
    // gop_size: GOP大小（I帧间隔）
    bool initialize(const std::string& codec_name,
                   int width, int height, int fps,
                   int bitrate, int gop_size = 30,
                   int sophon_idx = 0,
                   bool use_dma_buffer = false);
    
    // 编码一帧（输入OpenCV Mat，BGR格式）
    bool encodeFrame(const cv::Mat& frame);
    
    // 获取编码后的数据包
    bool getEncodedPacket(AVPacket* pkt);
    
    // 刷新编码器（获取剩余数据）
    bool flush();
    
    // 关闭编码器
    void close();
    
    // 检查是否已初始化
    bool isInitialized() const { return enc_ctx_ != nullptr; }
    
    // 获取编码器上下文（用于集成到其他编码流程）
    AVCodecContext* getCodecContext() { return enc_ctx_; }
    
private:
    AVCodecContext* enc_ctx_;
    AVCodec* encoder_;
    AVFrame* frame_;
    SwsContext* sws_ctx_;
    
    int width_;
    int height_;
    int fps_;
    int frame_count_;
    
    // 查找BM1684硬件编码器
    AVCodec* findBmEncoder(const std::string& codec_name);
    
    // 将OpenCV Mat转换为AVFrame（BGR -> YUV420P）
    bool convertMatToFrame(const cv::Mat& mat, AVFrame* av_frame);
};

} // namespace detector_service

#endif // ENABLE_BM1684

