#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <optional>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace detector_service {

/**
 * @brief RTMP推流器类
 * 用于将OpenCV Mat格式的视频帧推送到RTMP服务器
 */
class RTMPStreamer {
private:
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
    AVStream *video_stream;
    SwsContext *sws_ctx;
    int frame_count;
    int64_t start_pts;
    int64_t last_dts;  // 记录上一个DTS，确保单调递增
    int i_frame_count; // I帧计数
    int p_frame_count; // P帧计数
    
public:
    RTMPStreamer() : fmt_ctx(nullptr), codec_ctx(nullptr), 
                     video_stream(nullptr), sws_ctx(nullptr), frame_count(0),
                     start_pts(AV_NOPTS_VALUE), last_dts(-1), 
                     i_frame_count(0), p_frame_count(0) {}
    
    /**
     * @brief 初始化RTMP推流
     * @param rtmp_url RTMP推流地址，格式: rtmp://host:port/app/stream
     * @param width 视频宽度
     * @param height 视频高度
     * @param fps 帧率，默认25
     * @param bitrate 比特率（bps），可选，默认根据分辨率自动计算
     * @return 是否成功
     */
    bool initialize(const std::string& rtmp_url, int width, int height, 
                   int fps = 25, std::optional<int> bitrate = std::nullopt);
    
    /**
     * @brief 推送一帧视频
     * @param frame OpenCV Mat格式的视频帧（BGR格式）
     * @return 是否成功
     */
    bool pushFrame(const cv::Mat& frame);
    
    /**
     * @brief 关闭推流并清理资源
     */
    void close();
    
    /**
     * @brief 析构函数，自动清理资源
     */
    ~RTMPStreamer() {
        close();
    }
    
    /**
     * @brief 检查推流是否已初始化
     * @return 是否已初始化
     */
    bool isInitialized() const {
        return fmt_ctx != nullptr && codec_ctx != nullptr;
    }
};

} // namespace detector_service

