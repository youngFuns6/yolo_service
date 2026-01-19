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
#include "gb28181_streamer.h"
#include "gb28181_config.h"
#include "gb28181_sip_client.h"

#ifdef ENABLE_BM1684
#include "bm1684_video_decoder.h"
#include "yolov11_detector_bm1684.h"
#endif

namespace detector_service {

class StreamManager {
public:
    using FrameCallback = std::function<void(int channel_id, const cv::Mat& frame, 
                                            const std::vector<Detection>& detections)>;
    
    StreamManager();
    ~StreamManager();
    
    // 初始化（在数据库初始化后调用）
    void initialize();
    
    // 启动/停止拉流分析
    bool startAnalysis(int channel_id, std::shared_ptr<Channel> channel,
                      std::shared_ptr<YOLOv11Detector> detector);
#ifdef ENABLE_BM1684
    bool startAnalysisBM1684(int channel_id, std::shared_ptr<Channel> channel,
                            std::shared_ptr<YOLOv11DetectorBM1684> detector);
#endif
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
        
#ifdef ENABLE_BM1684
        std::unique_ptr<BM1684VideoDecoder> bm1684_decoder;  // BM1684硬件解码器
#endif
        
        AlgorithmConfig algorithm_config;  // 通道的算法配置
        std::mutex config_mutex;           // 配置更新锁
        std::vector<Detection> last_detections;  // 上一次的检测结果，用于避免跳帧时检测框闪烁
        
        // GB28181推流相关
        GB28181ChannelInfo gb28181_info;   // GB28181通道信息
        
        StreamContext() {}
    };
    
    void streamWorker(int channel_id, std::shared_ptr<Channel> channel,
                     std::shared_ptr<YOLOv11Detector> detector);
    
    std::mutex streams_mutex_;
    std::map<int, std::unique_ptr<StreamContext>> streams_;
    FrameCallback frame_callback_;
    
    // GB28181 SIP客户端
    std::unique_ptr<GB28181SipClient> gb28181_sip_client_;
    bool initGB28181SipClient();
    void handleGB28181Invite(const GB28181Session& session);
    void handleGB28181Bye(const std::string& channel_id);
};

} // namespace detector_service

