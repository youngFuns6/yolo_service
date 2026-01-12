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
#include "channel.h"
#include "yolov11_detector.h"
#include "algorithm_config.h"

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
    void streamWorker(int channel_id, std::shared_ptr<Channel> channel,
                     std::shared_ptr<YOLOv11Detector> detector);
    
    struct StreamContext {
        std::thread thread;
        std::atomic<bool> running;
        cv::VideoCapture cap;
        cv::VideoWriter writer;            // RTMP推流写入器
        bool push_stream_enabled;          // 是否启用推流
        int push_width;                    // 推流宽度
        int push_height;                   // 推流高度
        AlgorithmConfig algorithm_config;  // 通道的算法配置
        std::mutex config_mutex;           // 配置更新锁
    };
    
    std::mutex streams_mutex_;
    std::map<int, std::unique_ptr<StreamContext>> streams_;
    FrameCallback frame_callback_;
};

} // namespace detector_service

