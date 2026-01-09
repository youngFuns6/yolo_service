#include "stream_manager.h"
#include "config.h"
#include "database.h"
#include "channel.h"
#include "stream_config.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include "image_utils.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace detector_service {

/**
 * @brief 获取当前时间字符串
 */
std::string getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief 解码URL中的HTML实体编码
 * @param url 原始URL字符串
 * @return 解码后的URL字符串
 */
std::string decodeUrlEntities(const std::string& url) {
    std::string result = url;
    
    // 替换常见的HTML实体编码
    // &amp; -> &
    size_t pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
        result.replace(pos, 5, "&");
        pos += 1;
    }
    
    // 可以添加更多HTML实体解码，如：
    // &lt; -> <
    // &gt; -> >
    // &quot; -> "
    // &#39; -> '
    
    return result;
}

StreamManager::StreamManager() {
}

StreamManager::~StreamManager() {
    // 停止所有流
    std::lock_guard<std::mutex> lock(streams_mutex_);
    for (auto& pair : streams_) {
        stopAnalysis(pair.first);
    }
}


void StreamManager::setFrameCallback(FrameCallback callback) {
    frame_callback_ = callback;
    std::cout << "StreamManager: 帧回调函数已设置" << std::endl;
}


bool StreamManager::startAnalysis(int channel_id, std::shared_ptr<Channel> channel,
                                 std::shared_ptr<YOLOv11Detector> detector) {
    std::cout << "StreamManager: 开始启动分析，通道ID=" << channel_id << std::endl;
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    // 如果已经在运行，先停止
    if (streams_.find(channel_id) != streams_.end()) {
        std::cout << "StreamManager: 通道 " << channel_id << " 已在运行，先停止" << std::endl;
        stopAnalysis(channel_id);
    }
    
    auto context = std::make_unique<StreamContext>();
    context->running = true;
    
    // 解码URL中的HTML实体编码（如 &amp; -> &）
    std::string decoded_url = decodeUrlEntities(channel->source_url);
    
    // 打开视频源 - 使用FFmpeg后端处理RTSP流
    std::cout << "正在打开视频源进行分析: " << decoded_url << std::endl;
    context->cap.open(decoded_url, cv::CAP_FFMPEG);
    if (!context->cap.isOpened()) {
        std::cerr << "无法打开视频源: " << decoded_url << std::endl;
        // 更新状态为错误
        auto& db = Database::getInstance();
        std::string updated_at = getCurrentTime();
        db.updateChannelStatus(channel_id, "error", updated_at);
        return false;
    }
    
    // 拉流成功，更新状态为running
    auto& db = Database::getInstance();
    std::string updated_at = getCurrentTime();
    db.updateChannelStatus(channel_id, "running", updated_at);
    
    // 优化RTSP流设置以减少延迟和丢帧
    context->cap.set(cv::CAP_PROP_BUFFERSIZE, 1);  // 最小缓冲区，减少延迟
    context->cap.set(cv::CAP_PROP_FPS, channel->fps);
    // 设置 FFmpeg 选项以优化 RTSP 流读取
    context->cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('H', '2', '6', '4'));
    
    // 先将 context 插入到 streams_ 中，确保线程启动时可以访问
    streams_[channel_id] = std::move(context);
    
    // 启动工作线程（在插入 streams_ 之后，确保线程可以访问）
    streams_[channel_id]->thread = std::thread(&StreamManager::streamWorker, this,
                                                channel_id, channel, detector);
    
    std::cout << "StreamManager: 分析启动成功，通道ID=" << channel_id << std::endl;
    return true;
}

bool StreamManager::stopAnalysis(int channel_id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = streams_.find(channel_id);
    if (it == streams_.end()) {
        return false;
    }
    
    auto& context = it->second;
    context->running = false;
    
    if (context->thread.joinable()) {
        context->thread.join();
    }
    
    context->cap.release();
    
    streams_.erase(it);
    
    // 更新状态为stopped（如果enabled为false，则保持stopped；如果enabled为true，状态会在重新启动分析时更新）
    auto& db = Database::getInstance();
    auto& channel_manager = ChannelManager::getInstance();
    auto channel = channel_manager.getChannel(channel_id);
    if (channel && !channel->enabled.load()) {
        // 如果enabled为false，更新状态为stopped
        std::string updated_at = getCurrentTime();
        db.updateChannelStatus(channel_id, "stopped", updated_at);
    }
    
    return true;
}

bool StreamManager::isAnalyzing(int channel_id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(channel_id);
    return it != streams_.end() && it->second->running.load();
}

void StreamManager::streamWorker(int channel_id, std::shared_ptr<Channel> channel,
                                 std::shared_ptr<YOLOv11Detector> detector) {
    std::cout << "StreamManager: 启动工作线程，通道ID=" << channel_id 
              << ", 回调函数已设置=" << (frame_callback_ ? "是" : "否") << std::endl;
    
    // 获取 context 引用（需要加锁）
    std::unique_lock<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(channel_id);
    if (it == streams_.end()) {
        std::cerr << "StreamManager: 通道 " << channel_id << " 的 context 不存在" << std::endl;
        return;
    }
    auto& context = it->second;
    lock.unlock();  // 获取引用后释放锁，后续访问 context 的成员不需要锁（因为不会删除）
    
    cv::Mat frame, processed_frame;
    
    // 计算帧间隔
    int frame_interval = 1000 / channel->fps;
    auto last_time = std::chrono::steady_clock::now();
    
    // 帧跳过机制：不是每一帧都进行检测，降低处理负担
    // 检测频率：每 N 帧检测一次（例如每3帧检测1次）
    const int DETECTION_INTERVAL = 3;  // 每3帧检测一次
    int frame_counter = 0;  // 帧计数器
    
    // 重连相关变量
    int consecutive_failures = 0;
    const int MAX_CONSECUTIVE_FAILURES = 10;  // 连续失败10次后尝试重连
    const int RECONNECT_DELAY_MS = 2000;  // 重连前等待2秒
    
    // 重连辅助函数（lambda）
    auto reconnectStream = [&]() -> bool {
        std::cerr << "通道 " << channel_id << " 连续读取失败 " << consecutive_failures 
                  << " 次，尝试重新连接RTSP流..." << std::endl;
        
        // 释放当前连接
        context->cap.release();
        
        // 等待一段时间后重连
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
        
        // 解码URL并尝试重新打开流
        std::string decoded_url = decodeUrlEntities(channel->source_url);
        context->cap.open(decoded_url, cv::CAP_FFMPEG);
        if (context->cap.isOpened()) {
            // 重新设置参数
            context->cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
            context->cap.set(cv::CAP_PROP_FPS, channel->fps);
            context->cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('H', '2', '6', '4'));
            std::cout << "通道 " << channel_id << " RTSP流重连成功" << std::endl;
            consecutive_failures = 0;
            // 更新状态为running
            auto& db = Database::getInstance();
            std::string updated_at = getCurrentTime();
            db.updateChannelStatus(channel_id, "running", updated_at);
            return true;
        } else {
            std::cerr << "通道 " << channel_id << " RTSP流重连失败: " 
                      << decoded_url << std::endl;
            // 更新状态为error
            auto& db = Database::getInstance();
            std::string updated_at = getCurrentTime();
            db.updateChannelStatus(channel_id, "error", updated_at);
            return false;
        }
    };
    
    while (context->running.load()) {
        // 使用 grab() 快速跳过中间帧，只处理最新帧
        // 这样可以避免缓冲区堆积，减少"reader is too slow"的问题
        // 连续 grab 多次，只处理最后一帧（最多跳过4帧）
        int grab_count = 0;
        bool grab_success = true;
        while (grab_count < 4 && (grab_success = context->cap.grab())) {
            grab_count++;  // 跳过中间帧，只保留最新帧
        }
        
        // 如果最后一次 grab 失败，说明没有新帧，等待后继续
        if (!grab_success && grab_count == 0) {
            consecutive_failures++;
            if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                if (!reconnectStream()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        // 检查是否需要检测（降低检测频率）
        bool need_detection = (frame_counter % DETECTION_INTERVAL == 0);
        
        // retrieve 最新帧
        bool read_success = context->cap.retrieve(frame);
        
        if (!read_success || frame.empty()) {
            consecutive_failures++;
            
            if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                if (!reconnectStream()) {
                    // 重连失败，等待后继续尝试
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            } else {
                // 短暂等待后继续尝试
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        // 成功读取帧，重置失败计数
        consecutive_failures = 0;
        frame_counter++;
        
        // 调整大小
        if (frame.cols != channel->width || frame.rows != channel->height) {
            cv::resize(frame, frame, cv::Size(channel->width, channel->height));
        }
        
        // 使用检测器处理帧，生成分析后的帧
        // 只在需要时进行检测，降低处理负担
        std::vector<Detection> detections;
        if (detector && need_detection) {
            // 只在指定间隔时进行检测
            detections = detector->detect(frame);
            processed_frame = ImageUtils::drawDetections(frame, detections);
        } else {
            // 如果不需要检测或没有检测器，使用原始帧
            // 但如果有之前的检测结果，可以复用（这里简化处理，直接使用原始帧）
            processed_frame = frame.clone();
        }
        
        // 调用回调函数 - 无论是否有检测结果，都要发送帧数据
        if (frame_callback_) {
            try {
                frame_callback_(channel_id, processed_frame, detections);
            } catch (const std::exception& e) {
                std::cerr << "调用帧回调函数时发生异常: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "错误: 通道 " << channel_id << " 的帧回调函数未设置，无法发送分析流数据" << std::endl;
        }
        
        // 控制帧率
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_time).count();
        
        if (elapsed < frame_interval) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(frame_interval - elapsed));
        }
        
        last_time = std::chrono::steady_clock::now();
    }
}

} // namespace detector_service

