#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <map>
#include <optional>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include "channel.h"
#include "yolov11_detector.h"
#include "algorithm_config.h"
#include "rtmp_streamer.h"

namespace detector_service {

class StreamManager {
public:
    using FrameCallback = std::function<void(int channel_id, const cv::Mat& frame, 
                                            const std::vector<Detection>& detections)>;
    
    StreamManager();
    ~StreamManager();
    
    // 启动/停止拉流分析
    bool startAnalysis(int channel_id, std::shared_ptr<Channel> channel,
                      std::shared_ptr<YOLOv11Detector> detector);
    bool stopAnalysis(int channel_id);
    bool isAnalyzing(int channel_id);
    
    // 更新通道的算法配置（运行时更新）
    bool updateAlgorithmConfig(int channel_id, const AlgorithmConfig& config);
    
    // 设置帧回调
    void setFrameCallback(FrameCallback callback);

private:
    // StreamContext 结构体定义（需要在函数声明之前定义）
    struct StreamContext {
        std::thread thread;
        std::atomic<bool> running;
        cv::VideoCapture cap;
        
        // RTMP推流相关
        RTMPStreamer rtmp_streamer;        // RTMP推流器实例
        
        bool push_stream_enabled;          // 是否启用推流
        int push_width;                    // 推流宽度
        int push_height;                   // 推流高度
        AlgorithmConfig algorithm_config;  // 通道的算法配置
        std::mutex config_mutex;           // 配置更新锁
        std::vector<Detection> last_detections;  // 上一次的检测结果，用于避免跳帧时检测框闪烁
        
        // 推流重连相关
        bool push_reconnect_needed;        // 是否需要重连推流
        std::chrono::steady_clock::time_point push_reconnect_time;  // 重连时间点
        int push_reconnect_attempts;       // 重连尝试次数
        std::string push_rtmp_url;         // 推流RTMP地址（用于重连）
        int push_fps;                      // 推流帧率（用于重连）
        std::optional<int> push_bitrate;   // 推流比特率（用于重连）
        
        StreamContext() : push_stream_enabled(false), push_width(0), push_height(0),
                         push_reconnect_needed(false),
                         push_reconnect_time(std::chrono::steady_clock::now()),
                         push_reconnect_attempts(0), push_fps(0) {}
    };
    
    void streamWorker(int channel_id, std::shared_ptr<Channel> channel,
                     std::shared_ptr<YOLOv11Detector> detector);
    
    // FFmpeg推流相关辅助函数
    bool initFFmpegPushStream(StreamContext* context, 
                             const std::string& rtmp_url,
                             int width, int height, int fps, 
                             std::optional<int> bitrate);
    void cleanupFFmpegPushStream(StreamContext* context);
    
    std::mutex streams_mutex_;
    std::map<int, std::unique_ptr<StreamContext>> streams_;
    FrameCallback frame_callback_;
};

} // namespace detector_service

