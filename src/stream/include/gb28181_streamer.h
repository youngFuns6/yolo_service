#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <optional>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include "gb28181_config.h"

#ifdef ENABLE_BM1684
#include "bm1684_video_encoder.h"
#endif

namespace detector_service {

/**
 * @brief GB28181推流器类
 * 使用FFmpeg将视频流封装为PS/H.264格式，通过RTP发送
 */
class GB28181Streamer {
private:
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
    AVStream *video_stream;
    SwsContext *sws_ctx;
    
    int frame_count;
    int64_t start_pts;
    int64_t last_dts;
    
    // GB28181配置
    std::string ssrc;                       // 媒体流SSRC
    std::string dest_ip;                    // 目标IP（上级平台）
    int dest_port;                          // 目标RTP端口
    int local_port;                         // 本地RTP端口
    bool is_ps_stream;                      // 是否PS流（否则为H.264）
    
    std::atomic<bool> is_streaming;         // 是否正在推流
    std::mutex stream_mutex;
    
#ifdef ENABLE_BM1684
    std::unique_ptr<BM1684VideoEncoder> bm1684_encoder;  // BM1684硬件编码器
    bool use_hw_encode;                     // 是否使用硬件编码
#endif
    
public:
    GB28181Streamer() 
        : fmt_ctx(nullptr), 
          codec_ctx(nullptr), 
          video_stream(nullptr), 
          sws_ctx(nullptr), 
          frame_count(0),
          start_pts(AV_NOPTS_VALUE), 
          last_dts(-1),
          dest_port(0),
          local_port(0),
          is_ps_stream(true),
          is_streaming(false) {}
    
    /**
     * @brief 初始化GB28181推流
     * @param config GB28181配置
     * @param width 视频宽度
     * @param height 视频高度
     * @param fps 帧率
     * @param dest_ip 目标IP地址
     * @param dest_port 目标RTP端口
     * @param ssrc 媒体流SSRC
     * @param bitrate 比特率（bps），可选
     * @param use_hw_encode 是否使用硬件编码（BM1684），可选
     * @return 是否成功
     */
    bool initialize(const GB28181Config& config,
                   int width, int height, int fps,
                   const std::string& dest_ip, int dest_port,
                   const std::string& ssrc,
                   std::optional<int> bitrate = std::nullopt,
                   bool use_hw_encode = false);
    
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
    ~GB28181Streamer() {
        close();
    }
    
    /**
     * @brief 检查推流是否已初始化
     * @return 是否已初始化
     */
    bool isInitialized() const {
        return fmt_ctx != nullptr && codec_ctx != nullptr;
    }
    
    /**
     * @brief 检查是否正在推流
     * @return 是否正在推流
     */
    bool isStreaming() const {
        return is_streaming.load();
    }
    
    /**
     * @brief 获取当前SSRC
     * @return SSRC字符串
     */
    std::string getSSRC() const {
        return ssrc;
    }
};

/**
 * @brief GB28181通道信息
 * 存储每个通道的GB28181推流状态
 */
struct GB28181ChannelInfo {
    int channel_id;                         // 通道ID
    std::string channel_code;               // 通道编码（国标20位编码）
    std::shared_ptr<GB28181Streamer> streamer;  // 推流器
    bool is_active;                         // 是否激活
    std::string dest_ip;                    // 目标IP
    int dest_port;                          // 目标端口
    std::string ssrc;                       // SSRC
    
    GB28181ChannelInfo() 
        : channel_id(0),
          is_active(false),
          dest_port(0) {}
};

} // namespace detector_service

