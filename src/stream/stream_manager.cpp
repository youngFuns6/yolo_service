#include "stream_manager.h"
#include "config.h"
#include "database.h"
#include "channel.h"
#include "algorithm_config.h"
#include "gb28181_config.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include "image_utils.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cerrno>

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

/**
 * @brief 将FFmpeg错误码转换为字符串
 * @param errnum 错误码
 * @return 错误字符串
 */
std::string avErrorToString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

StreamManager::StreamManager() {
    // 初始化GB28181 SIP客户端
    initGB28181SipClient();
}

StreamManager::~StreamManager() {
    // 停止GB28181 SIP客户端
    if (gb28181_sip_client_) {
        gb28181_sip_client_->stop();
    }
    
    // 停止所有流
    std::unique_lock<std::mutex> lock(streams_mutex_);
    
    // 收集所有需要join的线程
    std::vector<std::thread> threads_to_join;
    std::vector<std::unique_ptr<StreamContext>> contexts_to_cleanup;
    
    for (auto& pair : streams_) {
        auto& context = pair.second;
        if (context) {
            // 停止运行标志
            context->running = false;
            
            // 保存线程以便后续join
            if (context->thread.joinable()) {
                threads_to_join.push_back(std::move(context->thread));
            }
            
            // 保存context以便后续清理
            contexts_to_cleanup.push_back(std::move(context));
        }
    }
    
    // 清空streams_，释放锁
    streams_.clear();
    lock.unlock();
    
    // 在锁外join所有线程
    for (auto& thread : threads_to_join) {
        try {
            if (thread.joinable()) {
                thread.join();
            }
        } catch (const std::system_error& e) {
            std::cerr << "等待线程结束时出错: " << e.what() << std::endl;
        }
    }
    
    // 在锁外清理资源
    for (auto& context : contexts_to_cleanup) {
        if (context) {
            context->cap.release();
        }
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
    
    // 优化RTSP流设置以提升稳定性和流畅度
    context->cap.set(cv::CAP_PROP_BUFFERSIZE, 1);  // 最小缓冲区（1帧），减少延迟，避免缓冲积压导致卡顿
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
    std::unique_lock<std::mutex> lock(streams_mutex_);
    
    auto it = streams_.find(channel_id);
    if (it == streams_.end()) {
        return false;
    }
    
    auto& context = it->second;
    context->running = false;
    
    // 保存线程引用，以便在释放锁后join
    std::thread thread_to_join = std::move(context->thread);
    
    // 释放锁后再join线程，避免死锁
    // 同时避免线程在持有锁时等待自己结束
    lock.unlock();
    
    if (thread_to_join.joinable()) {
        try {
            thread_to_join.join();
        } catch (const std::system_error& e) {
            std::cerr << "通道 " << channel_id << " 等待线程结束时出错: " << e.what() << std::endl;
        }
    }
    
    // 重新获取锁以继续清理
    lock.lock();
    
    // 重新查找（因为可能已经被其他操作修改）
    it = streams_.find(channel_id);
    if (it == streams_.end()) {
        return true;  // 已经被删除，说明清理完成
    }
    
    auto& context_ref = it->second;
    context_ref->cap.release();
    
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

bool StreamManager::updateAlgorithmConfig(int channel_id, const AlgorithmConfig& config) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(channel_id);
    if (it == streams_.end()) {
        return false;
    }
    
    // 更新配置
    {
        std::lock_guard<std::mutex> config_lock(it->second->config_mutex);
        it->second->algorithm_config = config;
    }
    
    std::cout << "StreamManager: 通道 " << channel_id << " 的算法配置已更新" << std::endl;
    return true;
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
    
    // 加载通道的算法配置
    {
        std::lock_guard<std::mutex> config_lock(context->config_mutex);
        auto& config_manager = AlgorithmConfigManager::getInstance();
        if (!config_manager.getAlgorithmConfig(channel_id, context->algorithm_config)) {
            std::cerr << "StreamManager: 无法加载通道 " << channel_id << " 的算法配置，使用默认配置" << std::endl;
            context->algorithm_config = config_manager.getDefaultConfig(channel_id);
        }
        
        // 根据配置更新检测器参数
        if (detector) {
            detector->updateConfThreshold(context->algorithm_config.conf_threshold);
            detector->updateNmsThreshold(context->algorithm_config.nms_threshold);
        }
    }
    
    // 初始化GB28181通道信息（如果启用）
    auto& gb28181_config_mgr = GB28181ConfigManager::getInstance();
    GB28181Config gb28181_config = gb28181_config_mgr.getGB28181Config();
    if (gb28181_config.enabled.load()) {
        // 生成通道编码（基于设备ID和通道ID）
        // GB28181通道编码格式：设备ID前10位 + 通道类型码(131/132/137等) + 通道序号(4位)
        std::string channel_code;
        if (gb28181_config.device_id.length() >= 10) {
            char channel_str[5];
            snprintf(channel_str, sizeof(channel_str), "%04d", channel_id);
            channel_code = gb28181_config.device_id.substr(0, 10) + "132" + std::string(channel_str) + 
                          gb28181_config.device_id.substr(gb28181_config.device_id.length() - 3);
        }
        
        context->gb28181_info.channel_id = channel_id;
        context->gb28181_info.channel_code = channel_code;
        context->gb28181_info.is_active = false;  // 初始状态未激活，等待上级平台请求
        
        std::cout << "StreamManager: 通道 " << channel_id 
                  << " GB28181已启用，通道编码: " << channel_code << std::endl;
    }
    
    cv::Mat frame, processed_frame;
    
    // 计算帧间隔
    int frame_interval = 1000 / channel->fps;
    auto last_time = std::chrono::steady_clock::now();
    
    // 帧跳过机制：不是每一帧都进行检测，降低处理负担
    // 检测频率：从算法配置中读取
    int detection_interval = 3;  // 默认值
    {
        std::lock_guard<std::mutex> config_lock(context->config_mutex);
        detection_interval = context->algorithm_config.detection_interval;
    }
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
            context->cap.set(cv::CAP_PROP_BUFFERSIZE, 1);  // 最小缓冲区，避免缓冲积压
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
        // *** 优化帧处理策略：不跳帧，确保推流流畅 ***
        // 直接读取每一帧，避免跳帧导致推流卡顿
        bool read_success = context->cap.read(frame);
        
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
        
        // 检查是否需要检测（降低检测频率）
        bool need_detection = (frame_counter % detection_interval == 0);
        
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
            
            // 应用算法配置的过滤（类别、ROI等）
            {
                std::lock_guard<std::mutex> config_lock(context->config_mutex);
                detections = detector->applyFilters(
                    detections,
                    context->algorithm_config.enabled_classes,
                    context->algorithm_config.rois,
                    frame.cols,
                    frame.rows
                );
            }
            
            // 保存检测结果，用于后续帧的显示
            context->last_detections = detections;
            
            processed_frame = ImageUtils::drawDetections(frame, detections);
        } else {
            // 如果不需要检测，使用上一次的检测结果来绘制检测框，避免闪烁
            if (detector && !context->last_detections.empty()) {
                // 使用上一次的检测结果绘制检测框
                processed_frame = ImageUtils::drawDetections(frame, context->last_detections);
                // 使用上一次的检测结果作为当前帧的检测结果（用于回调）
                detections = context->last_detections;
            } else {
                // 如果没有检测器或没有之前的检测结果，使用原始帧
                processed_frame = frame.clone();
            }
        }
        
        // GB28181推流处理（如果通道激活）
        if (context->gb28181_info.is_active && context->gb28181_info.streamer && 
            context->gb28181_info.streamer->isStreaming()) {
            // 推送处理后的帧到GB28181
            cv::Mat frame_to_push = processed_frame;
            
            // 如果GB28181配置有特定的尺寸要求，这里可以调整
            // 目前直接推送处理后的帧
            if (!context->gb28181_info.streamer->pushFrame(frame_to_push)) {
                // 推送失败，记录日志
                if (frame_counter % 100 == 0) {  // 每100帧打印一次，避免刷屏
                    std::cerr << "通道 " << channel_id << " GB28181推流失败" << std::endl;
                }
            }
        }
        
        // 调用回调函数 - 无论是否有检测结果，都要发送帧数据
        if (frame_callback_) {
            try {
                frame_callback_(channel_id, processed_frame, detections);
            } catch (const std::exception& e) {
                // 减少异常日志输出频率，避免日志刷屏
                if (frame_counter % 100 == 0) {
                    std::cerr << "调用帧回调函数时发生异常: " << e.what() << std::endl;
                }
            }
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

bool StreamManager::initGB28181SipClient() {
    auto& config_mgr = GB28181ConfigManager::getInstance();
    GB28181Config config = config_mgr.getGB28181Config();
    
    if (!config.enabled.load()) {
        std::cout << "GB28181: 未启用，跳过SIP客户端初始化" << std::endl;
        return false;
    }
    
    gb28181_sip_client_ = std::make_unique<GB28181SipClient>();
    
    if (!gb28181_sip_client_->initialize(config)) {
        std::cerr << "GB28181 SIP: 初始化失败" << std::endl;
        gb28181_sip_client_.reset();
        return false;
    }
    
    // 设置回调
    gb28181_sip_client_->setInviteCallback(
        [this](const GB28181Session& session) {
            handleGB28181Invite(session);
        }
    );
    
    gb28181_sip_client_->setByeCallback(
        [this](const std::string& channel_id) {
            handleGB28181Bye(channel_id);
        }
    );
    
    // 启动SIP客户端
    if (!gb28181_sip_client_->start()) {
        std::cerr << "GB28181 SIP: 启动失败" << std::endl;
        gb28181_sip_client_.reset();
        return false;
    }
    
    std::cout << "GB28181 SIP: 初始化并启动成功" << std::endl;
    return true;
}

void StreamManager::handleGB28181Invite(const GB28181Session& session) {
    std::cout << "GB28181: 收到Invite请求，通道=" << session.channel_id 
              << ", 目标=" << session.dest_ip << ":" << session.dest_port << std::endl;
    
    // 从通道编码中提取通道ID（简化版，假设通道编码的第14-17位是通道ID）
    int channel_id = 0;
    if (session.channel_id.length() >= 17) {
        try {
            std::string channel_str = session.channel_id.substr(13, 4);
            channel_id = std::stoi(channel_str);
        } catch (...) {
            std::cerr << "GB28181: 无法从通道编码解析通道ID: " << session.channel_id << std::endl;
            return;
        }
    }
    
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(channel_id);
    if (it == streams_.end() || !it->second) {
        std::cerr << "GB28181: 通道 " << channel_id << " 未在运行" << std::endl;
        return;
    }
    
    auto& context = it->second;
    
    // 初始化GB28181推流器
    if (!context->gb28181_info.streamer) {
        context->gb28181_info.streamer = std::make_unique<GB28181Streamer>();
    }
    
    // 获取通道信息
    auto& channel_mgr = ChannelManager::getInstance();
    auto channel = channel_mgr.getChannel(channel_id);
    if (!channel) {
        std::cerr << "GB28181: 无法获取通道 " << channel_id << " 的信息" << std::endl;
        return;
    }
    
    // 获取GB28181配置
    auto& config_mgr = GB28181ConfigManager::getInstance();
    GB28181Config gb28181_config = config_mgr.getGB28181Config();
    
    // 初始化推流器
    if (!context->gb28181_info.streamer->initialize(
            gb28181_config,
            channel->width,
            channel->height,
            channel->fps,
            session.dest_ip,
            session.dest_port,
            session.ssrc)) {
        std::cerr << "GB28181: 推流器初始化失败" << std::endl;
        return;
    }
    
    // 标记通道为活跃
    context->gb28181_info.is_active = true;
    
    // 发送200 OK
    if (gb28181_sip_client_) {
        gb28181_sip_client_->sendInviteOk(session);
    }
    
    std::cout << "GB28181: 通道 " << channel_id << " 推流已启动" << std::endl;
}

void StreamManager::handleGB28181Bye(const std::string& channel_id_str) {
    std::cout << "GB28181: 收到Bye请求，通道=" << channel_id_str << std::endl;
    
    // 从通道编码中提取通道ID
    int channel_id = 0;
    if (channel_id_str.length() >= 17) {
        try {
            std::string channel_str = channel_id_str.substr(13, 4);
            channel_id = std::stoi(channel_str);
        } catch (...) {
            std::cerr << "GB28181: 无法从通道编码解析通道ID: " << channel_id_str << std::endl;
            return;
        }
    }
    
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(channel_id);
    if (it == streams_.end() || !it->second) {
        return;
    }
    
    auto& context = it->second;
    
    // 停止推流
    if (context->gb28181_info.streamer) {
        context->gb28181_info.streamer->close();
    }
    
    // 标记通道为非活跃
    context->gb28181_info.is_active = false;
    
    std::cout << "GB28181: 通道 " << channel_id << " 推流已停止" << std::endl;
}

} // namespace detector_service

