#pragma once

#ifdef ENABLE_BM1684

#include <string>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// 定义 BM_PCIE_MODE（如果是 PCIE 模式，需要在编译时定义）
#ifndef BM_PCIE_MODE
// 默认 SOC 模式，如果是 PCIE 模式，需要在 CMakeLists.txt 中添加 -DBM_PCIE_MODE
#define BM_PCIE_MODE 0
#endif

namespace detector_service {

/**
 * @brief BM1684 硬件视频解码器封装
 * 基于 examples/multimedia/ff_video_decode 实现
 */
class BMVideoDecoder {
public:
    BMVideoDecoder();
    ~BMVideoDecoder();
    
    /**
     * @brief 打开视频解码器
     * @param filename 输入文件路径或 RTSP URL
     * @param codec_name 解码器名称（如 "h264_bm", "h265_bm"），为空则自动检测
     * @param output_format_mode 输出格式模式（101 表示 NV12）
     * @param extra_frame_buffer_num 额外帧缓冲区数量
     * @param sophon_idx TPU 设备索引
     * @param pcie_no_copyback PCIE 模式下是否启用零拷贝
     * @return 成功返回 true
     */
    bool openDec(const std::string& filename,
                 const std::string& codec_name = "",
                 int output_format_mode = 0,
                 int extra_frame_buffer_num = 2,
                 int sophon_idx = 0,
                 int pcie_no_copyback = 0);
    
    /**
     * @brief 关闭解码器
     */
    void closeDec();
    
    /**
     * @brief 获取一帧解码后的图像
     * @return AVFrame* 指针，失败返回 nullptr
     */
    AVFrame* grabFrame();
    
    /**
     * @brief 获取编解码参数
     */
    AVCodecParameters* getCodecPar() const { return video_dec_par_; }
    
    /**
     * @brief 获取视频宽度
     */
    int getWidth() const { return width_; }
    
    /**
     * @brief 获取视频高度
     */
    int getHeight() const { return height_; }
    
    /**
     * @brief 获取像素格式
     */
    int getPixFmt() const { return pix_fmt_; }

private:
    AVFormatContext* ifmt_ctx_;
    AVCodecContext* video_dec_ctx_;
    AVCodecParameters* video_dec_par_;
    AVCodec* decoder_;
    AVFrame* frame_;
    AVPacket pkt_;
    
    int width_;
    int height_;
    int pix_fmt_;
    int video_stream_idx_;
    int refcount_;
    
    AVCodec* findBmDecoder(AVCodecID dec_id, const std::string& name, 
                          int codec_name_flag, enum AVMediaType type);
    int openCodecContext(int* stream_idx, AVCodecContext** dec_ctx,
                        AVFormatContext* fmt_ctx, enum AVMediaType type,
                        int codec_name_flag, const std::string& coder_name,
                        int output_format_mode, int extra_frame_buffer_num,
                        int sophon_idx = 0, int pcie_no_copyback = 0);
};

/**
 * @brief BM1684 硬件视频编码器封装
 * 基于 examples/multimedia/ff_video_encode 实现
 */
class BMVideoEncoder {
public:
    BMVideoEncoder();
    ~BMVideoEncoder();
    
    /**
     * @brief 打开视频编码器
     * @param filename 输出文件路径或 RTSP URL
     * @param soc_idx TPU 设备索引
     * @param codec_id 编码器 ID (AV_CODEC_ID_H264 或 AV_CODEC_ID_H265)
     * @param framerate 帧率
     * @param width 宽度
     * @param height 高度
     * @param inputformat 输入像素格式 (AV_PIX_FMT_YUV420P 或 AV_PIX_FMT_NV12)
     * @param bitrate 比特率（bps）
     * @param roi_enable 是否启用 ROI
     * @return 成功返回 true
     */
    bool openEnc(const std::string& filename,
                int soc_idx,
                int codec_id,
                int framerate,
                int width,
                int height,
                int inputformat,
                int bitrate,
                int roi_enable = 0);
    
    /**
     * @brief 关闭编码器
     */
    void closeEnc();
    
    /**
     * @brief 写入一帧
     * @param data 图像数据指针
     * @param step 行步长（必须 32 字节对齐）
     * @param width 图像宽度
     * @param height 图像高度
     * @return 成功返回 0
     */
    int writeFrame(const uint8_t* data, int step, int width, int height);
    
    /**
     * @brief 刷新编码器缓冲区
     * @return 成功返回 0
     */
    int flushEncoder();

private:
    AVFormatContext* ofmt_ctx_;
    AVCodecContext* enc_ctx_;
    AVFrame* picture_;
    AVFrame* input_picture_;
    AVStream* out_stream_;
    uint8_t* aligned_input_;
    
    int frame_width_;
    int frame_height_;
    int frame_idx_;
    
    AVCodec* findHwVideoEncoder(int codecId);
};

} // namespace detector_service

#endif // ENABLE_BM1684
