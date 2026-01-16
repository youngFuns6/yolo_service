#include "ws_handler.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <nlohmann/json.hpp>
#include <iostream>
#include "image_utils.h"
#include "channel.h"

namespace detector_service {

WebSocketHandler::WebSocketHandler() : running_(true) {
    // 启动发送线程
    send_thread_ = std::thread(&WebSocketHandler::sendWorker, this);
}

WebSocketHandler::~WebSocketHandler() {
    // 停止发送线程
    running_ = false;
    frame_queue_cv_.notify_all();
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
}

void WebSocketHandler::handleChannelConnection(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    ConnectionInfo info;
    info.type = ConnectionType::CHANNEL;
    info.channel_id = -1;  // 初始值，等待客户端订阅
    connections_[&conn] = info;
}

void WebSocketHandler::handleAlertConnection(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    ConnectionInfo info;
    info.type = ConnectionType::ALERT;
    info.channel_id = -1;
    connections_[&conn] = info;
}

void WebSocketHandler::handleDisconnection(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(&conn);
    if (it != connections_.end()) {
        // 如果是通道订阅，从通道订阅列表中移除
        if (it->second.type == ConnectionType::CHANNEL && it->second.channel_id >= 0) {
            int channel_id = it->second.channel_id;
            auto& channel_conns = channel_subscriptions_[channel_id];
            channel_conns.erase(&conn);
            if (channel_conns.empty()) {
                channel_subscriptions_.erase(channel_id);
                // 清理该通道的FPS控制信息（如果没有订阅者了）
                std::lock_guard<std::mutex> fps_lock(channel_fps_controls_mutex_);
                channel_fps_controls_.erase(channel_id);
            }
        }
        connections_.erase(it);
    }
}

void WebSocketHandler::handleChannelMessage(crow::websocket::connection& conn, 
                                            const std::string& message, 
                                            bool /* is_binary */) {
    try {
        nlohmann::json msg = nlohmann::json::parse(message);
        
        if (msg.contains("action") && msg["action"] == "subscribe") {
            if (msg.contains("channel_id")) {
                int channel_id = msg["channel_id"];
                
                std::lock_guard<std::mutex> lock(connections_mutex_);
                auto it = connections_.find(&conn);
                if (it != connections_.end()) {
                    // 如果之前已订阅其他通道，先移除旧订阅
                    if (it->second.channel_id >= 0) {
                        auto& old_channel_conns = channel_subscriptions_[it->second.channel_id];
                        old_channel_conns.erase(&conn);
                        if (old_channel_conns.empty()) {
                            channel_subscriptions_.erase(it->second.channel_id);
                        }
                    }
                    
                    // 更新订阅信息
                    it->second.channel_id = channel_id;
                    channel_subscriptions_[channel_id].insert(&conn);
                    
                    // 发送确认消息
                    nlohmann::json response;
                    response["type"] = "subscription_confirmed";
                    response["channel_id"] = channel_id;
                    conn.send_text(response.dump());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "处理通道订阅消息失败: " << e.what() << std::endl;
    }
}

void WebSocketHandler::handleAlertMessage(crow::websocket::connection& conn, 
                                         const std::string& /* message */, 
                                         bool /* is_binary */) {
    // 报警连接不需要处理消息，连接即表示订阅
    // 可以在这里发送确认消息
    nlohmann::json response;
    response["type"] = "alert_subscription_confirmed";
    conn.send_text(response.dump());
}

void WebSocketHandler::broadcastAlert(const AlertMessage& alert) {
    std::string json = alertToJson(alert);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (const auto& pair : connections_) {
        if (pair.second.type == ConnectionType::ALERT && pair.first) {
            try {
                pair.first->send_text(json);
            } catch (const std::exception& e) {
                std::cerr << "发送报警消息失败: " << e.what() << std::endl;
            }
        }
    }
}

void WebSocketHandler::broadcastFrame(int channel_id, const cv::Mat& frame) {
    // 使用丢帧机制：只保留每个通道的最新帧
    {
        std::lock_guard<std::mutex> lock(latest_frames_mutex_);
        FrameData frame_data;
        frame_data.channel_id = channel_id;
        frame_data.frame = frame.clone();  // 克隆帧数据
        frame_data.timestamp = std::chrono::steady_clock::now();
        latest_frames_[channel_id] = std::move(frame_data);
    }
    
    // 初始化或更新通道的FPS控制信息
    {
        std::lock_guard<std::mutex> fps_lock(channel_fps_controls_mutex_);
        auto& channel_manager = ChannelManager::getInstance();
        auto channel = channel_manager.getChannel(channel_id);
        if (channel) {
            auto it = channel_fps_controls_.find(channel_id);
            if (it == channel_fps_controls_.end()) {
                // 首次初始化
                ChannelFpsControl control;
                control.fps = channel->fps > 0 ? channel->fps : 25;  // 默认25fps
                control.last_send_time = std::chrono::steady_clock::now();
                channel_fps_controls_[channel_id] = control;
            } else {
                // 更新FPS（如果通道配置改变）
                it->second.fps = channel->fps > 0 ? channel->fps : 25;
            }
        }
    }
    
    // 通知发送线程有新帧
    frame_queue_cv_.notify_one();
}

void WebSocketHandler::sendWorker() {
    // 添加帧率控制，根据通道FPS限制发送频率，避免浏览器来不及渲染
    
    while (running_) {
        std::unique_lock<std::mutex> lock(latest_frames_mutex_);
        
        // 等待新帧或停止信号
        frame_queue_cv_.wait(lock, [this] {
            return !latest_frames_.empty() || !running_;
        });
        
        if (!running_) {
            break;
        }
        
        // 获取所有通道的最新帧
        std::map<int, FrameData> frames_to_send = latest_frames_;
        latest_frames_.clear();  // 清空，等待新帧
        lock.unlock();
        
        // 发送每个通道的最新帧（带帧率控制）
        for (const auto& pair : frames_to_send) {
            int channel_id = pair.first;
            const FrameData& frame_data = pair.second;
            
            // 检查是否有订阅者（快速检查，避免不必要的编码）
            {
                std::lock_guard<std::mutex> conn_lock(connections_mutex_);
                auto it = channel_subscriptions_.find(channel_id);
                if (it == channel_subscriptions_.end() || it->second.empty()) {
                    continue;  // 没有订阅者，跳过编码和发送
                }
            }
            
            // 帧率控制：检查是否应该发送这一帧
            {
                std::lock_guard<std::mutex> fps_lock(channel_fps_controls_mutex_);
                auto fps_it = channel_fps_controls_.find(channel_id);
                if (fps_it != channel_fps_controls_.end()) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - fps_it->second.last_send_time).count();
                    
                    // 计算帧间隔（微秒）
                    int64_t frame_interval_us = 1000000LL / fps_it->second.fps;
                    
                    // 如果距离上次发送时间太短，跳过这一帧（丢帧以保持流畅）
                    if (elapsed < frame_interval_us) {
                        continue;  // 跳过这一帧，等待下一帧
                    }
                    
                    // 更新上次发送时间
                    fps_it->second.last_send_time = now;
                }
            }
            
            // 编码帧数据（在锁外执行，避免阻塞其他通道）
            // 注意：编码是耗时操作，但必须同步执行以确保数据一致性
            std::string json = frameToJson(channel_id, frame_data.frame);
            
            // 发送给所有订阅者
            std::lock_guard<std::mutex> conn_lock(connections_mutex_);
            auto it = channel_subscriptions_.find(channel_id);
            if (it == channel_subscriptions_.end() || it->second.empty()) {
                continue;  // 订阅者可能在编码期间断开
            }
            
            std::vector<crow::websocket::connection*> to_remove;
            for (auto* conn : it->second) {
                if (conn) {
                    try {
                        conn->send_text(json);
                    } catch (const std::exception& e) {
                        std::cerr << "发送帧数据失败 (通道 " << channel_id << "): " 
                                  << e.what() << std::endl;
                        // 标记为需要移除的连接
                        to_remove.push_back(conn);
                    }
                } else {
                    to_remove.push_back(conn);
                }
            }
            
            // 移除失效的连接
            for (auto* conn : to_remove) {
                it->second.erase(conn);
                connections_.erase(conn);
            }
            
            if (it->second.empty()) {
                channel_subscriptions_.erase(it);
            }
        }
    }
}

std::string WebSocketHandler::alertToJson(const AlertMessage& alert) {
    nlohmann::json j;
    j["type"] = "alert";
    j["channel_id"] = alert.channel_id;
    j["channel_name"] = alert.channel_name;
    j["alert_type"] = alert.alert_type;
    j["image_base64"] = alert.image_base64;
    j["confidence"] = alert.confidence;
    j["detected_objects"] = alert.detected_objects;
    j["timestamp"] = alert.timestamp;
    return j.dump();
}

std::string WebSocketHandler::frameToJson(int channel_id, const cv::Mat& frame) {
    // 使用较低的 JPEG 质量参数（60）以减少编码时间和数据大小，降低延迟
    // 对于实时视频流，60 的质量已经足够清晰
    std::string image_base64 = ImageUtils::matToBase64(frame, ".jpg", 60);
    
    nlohmann::json j;
    j["type"] = "frame";
    j["channel_id"] = channel_id;
    j["image_base64"] = image_base64;
    
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    j["timestamp"] = oss.str();
    
    return j.dump();
}

} // namespace detector_service

