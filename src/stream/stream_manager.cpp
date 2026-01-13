#include "stream_manager.h"
#include "config.h"
#include "database.h"
#include "channel.h"
#include "push_stream_config.h"
#include "algorithm_config.h"
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

/**
 * @brief 初始化FFmpeg推流
 * @param context StreamContext指针
 * @param rtmp_url RTMP推流地址
 * @param width 推流宽度
 * @param height 推流高度
 * @param fps 帧率
 * @param bitrate 比特率（bps，可选）
 * @return 是否成功
 */
bool StreamManager::initFFmpegPushStream(StreamContext* context, 
                         const std::string& rtmp_url,
                         int width, int height, int fps, 
                         std::optional<int> bitrate) {
    if (!context) {
        return false;
    }
    
    // 使用 RTMPStreamer 初始化推流
    return context->rtmp_streamer.initialize(rtmp_url, width, height, fps, bitrate);
}

/**
 * @brief 清理FFmpeg推流资源
 * @param context StreamContext指针
 */
void StreamManager::cleanupFFmpegPushStream(StreamContext* context) {
    if (!context) {
        return;
    }
    
    // 使用 RTMPStreamer 关闭推流
    if (context->rtmp_streamer.isInitialized()) {
        context->rtmp_streamer.close();
    }
}

StreamManager::StreamManager() {
}

StreamManager::~StreamManager() {
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
            
            // 停止重连尝试
            context->push_reconnect_needed = false;
            context->push_reconnect_attempts = 0;
            
            // 释放推流资源
            if (context->push_stream_enabled && context->rtmp_streamer.isInitialized()) {
                cleanupFFmpegPushStream(context.get());
            }
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
    context->push_stream_enabled = false;  // 初始化为false，在streamWorker中根据通道状态设置
    context->push_width = 0;
    context->push_height = 0;
    
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
    
    // 停止重连尝试
    context_ref->push_reconnect_needed = false;
    context_ref->push_reconnect_attempts = 0;
    
    // 释放推流资源
    if (context_ref->push_stream_enabled && context_ref->rtmp_streamer.isInitialized()) {
        cleanupFFmpegPushStream(context_ref.get());
        std::cout << "通道 " << channel_id << " RTMP推流已停止" << std::endl;
    }
    
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
    
    // 初始化推流（如果通道推流状态开启）
    context->push_stream_enabled = channel->push_enabled.load();
    context->push_width = 0;
    context->push_height = 0;
    if (context->push_stream_enabled) {
        auto& push_stream_config_mgr = PushStreamConfigManager::getInstance();
        PushStreamConfig push_config = push_stream_config_mgr.getPushStreamConfig();
        
        if (!push_config.rtmp_url.empty()) {
            // 确定推流的尺寸和帧率
            context->push_width = push_config.width.value_or(channel->width);
            context->push_height = push_config.height.value_or(channel->height);
            int push_fps = push_config.fps.value_or(channel->fps);
            
            // 验证推流配置的有效性
            if (context->push_width <= 0 || context->push_height <= 0) {
                std::cerr << "通道 " << channel_id << " RTMP推流启动失败: 推流尺寸无效 ("
                          << context->push_width << "x" << context->push_height << ")" << std::endl;
                context->push_stream_enabled = false;
            } else if (push_fps <= 0) {
                std::cerr << "通道 " << channel_id << " RTMP推流启动失败: 帧率无效 (" << push_fps << ")" << std::endl;
                context->push_stream_enabled = false;
            } else {
                // 使用FFmpeg库进行RTMP推流
                std::string output_url = push_config.rtmp_url;
                
                // 保存推流配置用于重连
                context->push_rtmp_url = output_url;
                context->push_fps = push_fps;
                context->push_bitrate = push_config.bitrate;
                context->push_reconnect_attempts = 0;
                context->push_reconnect_needed = false;
                
                std::cout << "通道 " << channel_id << " 正在尝试启动RTMP推流: " << output_url 
                          << " (尺寸: " << context->push_width << "x" << context->push_height 
                          << ", 帧率: " << push_fps;
                if (push_config.bitrate.has_value() && push_config.bitrate.value() > 0) {
                    std::cout << ", 比特率: " << (push_config.bitrate.value() / 1000) << "kbps";
                }
                std::cout << ")" << std::endl;
                
                // 初始化FFmpeg推流
                if (initFFmpegPushStream(context.get(), output_url, 
                                        context->push_width, context->push_height, 
                                        push_fps, push_config.bitrate)) {
                    std::cout << "通道 " << channel_id << " RTMP推流已启动: " << output_url 
                              << " (尺寸: " << context->push_width << "x" << context->push_height 
                              << ", 帧率: " << push_fps << ")" << std::endl;
                } else {
                    std::cerr << "通道 " << channel_id << " RTMP推流启动失败: " << output_url << std::endl;
                    std::cerr << "  可能的原因：" << std::endl;
                    std::cerr << "  1. RTMP服务器地址不可达或配置错误" << std::endl;
                    std::cerr << "  2. FFmpeg库初始化失败" << std::endl;
                    std::cerr << "  3. 推流配置参数不正确（尺寸: " << context->push_width 
                              << "x" << context->push_height << ", 帧率: " << push_fps << ")" << std::endl;
                    std::cerr << "  4. 网络连接问题或RTMP服务器未运行" << std::endl;
                    // 标记需要重连（如果通道推流仍然启用）
                    if (channel->push_enabled.load()) {
                        context->push_reconnect_needed = true;
                        context->push_reconnect_time = std::chrono::steady_clock::now() + 
                                                       std::chrono::seconds(5);  // 5秒后重连
                        context->push_reconnect_attempts = 0;
                    } else {
                        context->push_stream_enabled = false;
                    }
                }
            }
        } else {
            std::cerr << "通道 " << channel_id << " 推流已启用，但未配置RTMP地址" << std::endl;
            context->push_stream_enabled = false;
        }
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
        
        // 检查是否需要重连推流
        if (context->push_reconnect_needed) {
            // 如果通道推流已被禁用，停止重连
            if (!channel->push_enabled.load()) {
                std::cout << "通道 " << channel_id << " 推流已被禁用，停止重连尝试" << std::endl;
                context->push_reconnect_needed = false;
                context->push_stream_enabled = false;
                context->push_reconnect_attempts = 0;
            } else {
                auto current_time = std::chrono::steady_clock::now();
                if (current_time >= context->push_reconnect_time) {
                    std::cout << "通道 " << channel_id << " 开始尝试重连RTMP推流 (第 " 
                              << context->push_reconnect_attempts << " 次)..." << std::endl;
                    
                    // 尝试重新初始化推流
                    if (initFFmpegPushStream(context.get(), context->push_rtmp_url,
                                            context->push_width, context->push_height,
                                            context->push_fps, context->push_bitrate)) {
                        std::cout << "通道 " << channel_id << " RTMP推流重连成功" << std::endl;
                        context->push_reconnect_needed = false;
                        context->push_reconnect_attempts = 0;  // 重置重连计数
                        context->push_stream_enabled = true;
                    } else {
                        std::cerr << "通道 " << channel_id << " RTMP推流重连失败，将在5秒后再次尝试" << std::endl;
                        // 更新重连时间，延长重连间隔（指数退避）
                        int delay_seconds = std::min(5 + context->push_reconnect_attempts * 2, 30);  // 最多30秒
                        context->push_reconnect_time = current_time + std::chrono::seconds(delay_seconds);
                    }
                }
            }
        }
        
        // 推流处理：如果通道推流状态开启，则进行推流
        if (context->push_stream_enabled && context->rtmp_streamer.isInitialized()) {
            // 根据检测状态决定推流内容
            cv::Mat frame_to_push;
            // 如果通道启用了检测功能（enabled=true），推送分析后的视频数据
            // 如果检测器存在且进行了检测，processed_frame 包含检测框；否则是原始帧的克隆
            // 如果通道未启用检测，推送原始通道视频数据
            if (channel->enabled.load() && detector) {
                // 检测开启，推送分析后的视频数据（带检测框的帧或原始帧）
                frame_to_push = processed_frame;
            } else {
                // 检测未开启，推送原始通道视频数据
                frame_to_push = frame;
            }
            
            // 如果推流尺寸与帧尺寸不一致，需要调整大小
            cv::Mat resized_frame;
            if (context->push_width > 0 && context->push_height > 0 && 
                (frame_to_push.cols != context->push_width || frame_to_push.rows != context->push_height)) {
                cv::resize(frame_to_push, resized_frame, cv::Size(context->push_width, context->push_height));
                frame_to_push = resized_frame;
            }
            
            // 使用 RTMPStreamer 推送帧
            // 确保帧格式和尺寸正确
            if (frame_to_push.empty()) {
                std::cerr << "通道 " << channel_id << " 推流帧为空，跳过" << std::endl;
            } else if (frame_to_push.type() != CV_8UC3) {
                std::cerr << "通道 " << channel_id << " 推流帧格式错误: 期望 CV_8UC3, 实际 " 
                          << frame_to_push.type() << "，跳过" << std::endl;
            } else if (frame_to_push.cols != context->push_width || 
                       frame_to_push.rows != context->push_height) {
                std::cerr << "通道 " << channel_id << " 推流帧尺寸不匹配: 期望 " 
                          << context->push_width << "x" << context->push_height 
                          << ", 实际 " << frame_to_push.cols << "x" << frame_to_push.rows 
                          << "，跳过" << std::endl;
            } else {
                if (!context->rtmp_streamer.pushFrame(frame_to_push)) {
                    // 推送失败，可能是连接断开
                    std::cerr << "通道 " << channel_id << " RTMP推流推送帧失败，将尝试重连" << std::endl;
                    
                    // 清理推流资源
                    cleanupFFmpegPushStream(context.get());
                    
                    // 如果通道推流仍然启用，标记需要重连
                    if (channel->push_enabled.load()) {
                        context->push_reconnect_needed = true;
                        context->push_reconnect_time = std::chrono::steady_clock::now() + 
                                                       std::chrono::seconds(3);  // 3秒后重连
                        context->push_reconnect_attempts++;
                        
                        // 限制重连次数，避免无限重连
                        const int MAX_RECONNECT_ATTEMPTS = 10;
                        if (context->push_reconnect_attempts > MAX_RECONNECT_ATTEMPTS) {
                            std::cerr << "通道 " << channel_id << " RTMP推流重连次数超过限制（" 
                                      << MAX_RECONNECT_ATTEMPTS << "次），停止重连" << std::endl;
                            context->push_stream_enabled = false;
                            context->push_reconnect_needed = false;
                        } else {
                            std::cout << "通道 " << channel_id << " 将在3秒后尝试第 " 
                                      << context->push_reconnect_attempts << " 次重连" << std::endl;
                        }
                    } else {
                        // 通道推流已禁用，不再重连
                        context->push_stream_enabled = false;
                        context->push_reconnect_needed = false;
                    }
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
} // namespace detector_service

