#pragma once

#include <crow.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <set>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include "image_utils.h"

namespace detector_service {

struct AlertMessage {
    int channel_id;
    std::string channel_name;
    std::string alert_type;
    std::string image_base64;
    float confidence;
    std::string detected_objects;  // JSON string
    std::string timestamp;
};

enum class ConnectionType {
    CHANNEL,  // 通道数据订阅
    ALERT     // 报警数据订阅
};

struct ConnectionInfo {
    ConnectionType type;
    int channel_id;  // 仅用于 CHANNEL 类型
};

// 帧数据结构，用于缓冲队列
struct FrameData {
    int channel_id;
    cv::Mat frame;
    std::chrono::steady_clock::time_point timestamp;
};

class WebSocketHandler {
public:
    static WebSocketHandler& getInstance() {
        static WebSocketHandler instance;
        return instance;
    }
    
    // 发送报警信息（仅发送给报警订阅连接）
    void broadcastAlert(const AlertMessage& alert);
    
    // 发送图片帧（仅发送给订阅了对应通道的连接）
    void broadcastFrame(int channel_id, const cv::Mat& frame);
    
    // 处理 WebSocket 连接
    void handleChannelConnection(crow::websocket::connection& conn);
    void handleAlertConnection(crow::websocket::connection& conn);
    void handleDisconnection(crow::websocket::connection& conn);
    void handleChannelMessage(crow::websocket::connection& conn, const std::string& message, bool is_binary);
    void handleAlertMessage(crow::websocket::connection& conn, const std::string& message, bool is_binary);

private:
    WebSocketHandler();
    ~WebSocketHandler();
    WebSocketHandler(const WebSocketHandler&) = delete;
    WebSocketHandler& operator=(const WebSocketHandler&) = delete;
    
    // 发送线程工作函数
    void sendWorker();
    
    std::mutex connections_mutex_;
    // 存储连接和其订阅信息
    std::map<crow::websocket::connection*, ConnectionInfo> connections_;
    // 按通道ID索引的连接集合，用于快速查找
    std::map<int, std::set<crow::websocket::connection*>> channel_subscriptions_;
    
    // 帧缓冲相关
    std::condition_variable frame_queue_cv_;
    std::atomic<bool> running_;
    std::thread send_thread_;
    
    // 每个通道的最新帧（用于丢帧机制）
    std::map<int, FrameData> latest_frames_;
    std::mutex latest_frames_mutex_;
    
    std::string alertToJson(const AlertMessage& alert);
    std::string frameToJson(int channel_id, const cv::Mat& frame);
};

} // namespace detector_service

