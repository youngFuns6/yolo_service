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
    
    // 设置帧回调
    void setFrameCallback(FrameCallback callback);

private:
    void streamWorker(int channel_id, std::shared_ptr<Channel> channel,
                     std::shared_ptr<YOLOv11Detector> detector);
    
    struct StreamContext {
        std::thread thread;
        std::atomic<bool> running;
        cv::VideoCapture cap;
    };
    
    std::mutex streams_mutex_;
    std::map<int, std::unique_ptr<StreamContext>> streams_;
    FrameCallback frame_callback_;
};

} // namespace detector_service

