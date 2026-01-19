#pragma once

#ifdef ENABLE_BM1684

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

namespace detector_service {

class BM1684VideoDecoder {
public:
    BM1684VideoDecoder();
    ~BM1684VideoDecoder();
    
    // 打开视频流（使用BM1684硬件解码器）
    // codec_name: "h264_bm", "h265_bm" 等
    // sophon_idx: BM1684设备索引
    // pcie_no_copyback: PCIe模式下是否启用零拷贝
    bool open(const std::string& url, 
              const std::string& codec_name = "h264_bm",
              int sophon_idx = 0,
              int pcie_no_copyback = 0);
    
    void close();
    
    // 读取一帧（返回OpenCV Mat格式）
    bool read(cv::Mat& frame);
    
    // 检查是否已打开
    bool isOpened() const { return video_dec_ctx_ != nullptr; }
    
    // 获取视频信息
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getFPS() const;
    
private:
    AVFormatContext* ifmt_ctx_;
    AVCodecContext* video_dec_ctx_;
    AVCodecParameters* video_dec_par_;
    AVCodec* decoder_;
    
    int width_;
    int height_;
    int pix_fmt_;
    int video_stream_idx_;
    
    AVFrame* frame_;
    AVPacket pkt_;
    
    // 查找BM1684解码器
    AVCodec* findBmDecoder(AVCodecID dec_id, const std::string& name);
    
    // 打开编解码上下文
    int openCodecContext(int* stream_idx,
                        AVCodecContext** dec_ctx,
                        AVFormatContext* fmt_ctx,
                        const std::string& codec_name,
                        int sophon_idx,
                        int pcie_no_copyback);
    
    // 将AVFrame转换为cv::Mat
    bool avFrameToMat(AVFrame* av_frame, cv::Mat& mat);
};

} // namespace detector_service

#endif // ENABLE_BM1684

