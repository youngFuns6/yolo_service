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
    
    // 初始化FFmpeg（如果还没有初始化）
    avformat_network_init();
    
    // 分配输出格式上下文
    int ret = avformat_alloc_output_context2(&context->fmt_ctx, nullptr, "flv", rtmp_url.c_str());
    if (ret < 0 || !context->fmt_ctx) {
        std::cerr << "无法创建输出格式上下文: " << avErrorToString(ret) << std::endl;
        return false;
    }
    
    // 查找H.264编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "找不到H.264编码器" << std::endl;
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 创建编码器上下文
    context->codec_ctx = avcodec_alloc_context3(codec);
    if (!context->codec_ctx) {
        std::cerr << "无法分配编码器上下文" << std::endl;
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 设置编码器参数
    context->codec_ctx->codec_id = AV_CODEC_ID_H264;
    context->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    context->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    context->codec_ctx->width = width;
    context->codec_ctx->height = height;
    context->codec_ctx->time_base = {1, fps};
    context->codec_ctx->framerate = {fps, 1};
    context->codec_ctx->gop_size = fps * 2;  // 关键帧间隔：2秒
    context->codec_ctx->max_b_frames = 0;    // 不使用B帧，降低延迟
    
    // 设置比特率
    if (bitrate.has_value() && bitrate.value() > 0) {
        context->codec_ctx->bit_rate = bitrate.value();
    } else {
        // 默认比特率：根据分辨率估算
        context->codec_ctx->bit_rate = width * height * fps / 10;
    }
    
    // 设置编码器预设（快速编码，低延迟）
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "ultrafast", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);
    av_dict_set(&opts, "profile", "baseline", 0);
    // 强制第一个帧为关键帧
    av_dict_set(&opts, "force_key_frames", "expr:gte(n,0)", 0);
    
    // 打开编码器
    ret = avcodec_open2(context->codec_ctx, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        std::cerr << "无法打开编码器: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 创建输出流
    AVStream* stream = avformat_new_stream(context->fmt_ctx, codec);
    if (!stream) {
        std::cerr << "无法创建输出流" << std::endl;
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 复制编码器参数到流
    ret = avcodec_parameters_from_context(stream->codecpar, context->codec_ctx);
    if (ret < 0) {
        std::cerr << "无法复制编码器参数: " << avErrorToString(ret) << std::endl;
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 设置流的时间基与编码器一致
    stream->time_base = context->codec_ctx->time_base;
    
    // 分配帧（在打开连接之前分配，用于生成extradata）
    context->frame = av_frame_alloc();
    context->frame_yuv = av_frame_alloc();
    context->pkt = av_packet_alloc();
    
    if (!context->frame || !context->frame_yuv || !context->pkt) {
        std::cerr << "无法分配帧或数据包" << std::endl;
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 设置帧参数
    context->frame->format = AV_PIX_FMT_BGR24;  // OpenCV使用BGR格式
    context->frame->width = width;
    context->frame->height = height;
    ret = av_frame_get_buffer(context->frame, 32);
    if (ret < 0) {
        std::cerr << "无法为帧分配缓冲区: " << avErrorToString(ret) << std::endl;
        av_frame_free(&context->frame);
        av_frame_free(&context->frame_yuv);
        av_packet_free(&context->pkt);
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    context->frame_yuv->format = AV_PIX_FMT_YUV420P;
    context->frame_yuv->width = width;
    context->frame_yuv->height = height;
    ret = av_frame_get_buffer(context->frame_yuv, 32);
    if (ret < 0) {
        std::cerr << "无法为YUV帧分配缓冲区: " << avErrorToString(ret) << std::endl;
        av_frame_free(&context->frame);
        av_frame_free(&context->frame_yuv);
        av_packet_free(&context->pkt);
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 创建颜色空间转换上下文（BGR -> YUV420P）
    context->sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!context->sws_ctx) {
        std::cerr << "无法创建颜色空间转换上下文" << std::endl;
        av_frame_free(&context->frame);
        av_frame_free(&context->frame_yuv);
        av_packet_free(&context->pkt);
        avcodec_free_context(&context->codec_ctx);
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
        return false;
    }
    
    // 生成extradata：编码一个测试帧来触发编码器生成SPS/PPS
    // 创建一个全黑的测试帧
    memset(context->frame_yuv->data[0], 0, context->frame_yuv->linesize[0] * height);
    memset(context->frame_yuv->data[1], 128, context->frame_yuv->linesize[1] * height / 2);
    memset(context->frame_yuv->data[2], 128, context->frame_yuv->linesize[2] * height / 2);
    
    // 测试帧的PTS设置为AV_NOPTS_VALUE，因为这只是用来生成extradata的
    context->frame_yuv->pts = AV_NOPTS_VALUE;
    context->frame_yuv->pict_type = AV_PICTURE_TYPE_I;  // 强制为关键帧
    
    // 发送测试帧到编码器
    ret = avcodec_send_frame(context->codec_ctx, context->frame_yuv);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        std::cerr << "发送测试帧失败: " << avErrorToString(ret) << std::endl;
        cleanupFFmpegPushStream(context);
        return false;
    }
    
    // 接收编码后的数据包（用于生成extradata）
    bool extradata_generated = false;
    while (ret >= 0) {
        ret = avcodec_receive_packet(context->codec_ctx, context->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            break;
        }
        
        // 如果编码器已经生成了extradata，更新流参数
        if (context->codec_ctx->extradata && context->codec_ctx->extradata_size > 0) {
            // 更新流参数，确保extradata正确设置
            ret = avcodec_parameters_from_context(stream->codecpar, context->codec_ctx);
            if (ret < 0) {
                std::cerr << "无法更新编码器参数: " << avErrorToString(ret) << std::endl;
                av_packet_unref(context->pkt);
                cleanupFFmpegPushStream(context);
                return false;
            }
            extradata_generated = true;
        }
        
        av_packet_unref(context->pkt);
    }
    
    // 如果extradata还没有生成，尝试从编码器上下文直接获取
    if (!extradata_generated && context->codec_ctx->extradata && context->codec_ctx->extradata_size > 0) {
        ret = avcodec_parameters_from_context(stream->codecpar, context->codec_ctx);
        if (ret < 0) {
            std::cerr << "无法更新编码器参数: " << avErrorToString(ret) << std::endl;
            cleanupFFmpegPushStream(context);
            return false;
        }
        extradata_generated = true;
    }
    
    // 验证extradata是否已设置
    if (!stream->codecpar->extradata || stream->codecpar->extradata_size == 0) {
        std::cerr << "警告: 编码器未生成extradata，可能导致RTMP服务器无法解析H264配置" << std::endl;
        // 继续执行，让FFmpeg在写入文件头时处理
    } else {
        std::cout << "H264 extradata已生成 (大小: " << stream->codecpar->extradata_size << " 字节)" << std::endl;
    }
    
    // 打开输出URL
    if (!(context->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        std::cout << "正在连接到RTMP服务器: " << rtmp_url << std::endl;
        ret = avio_open(&context->fmt_ctx->pb, rtmp_url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::string error_msg = avErrorToString(ret);
            std::cerr << "无法打开输出URL: " << error_msg << " (错误码: " << ret << ")" << std::endl;
            std::cerr << "RTMP连接失败的可能原因：" << std::endl;
            std::cerr << "  1. RTMP服务器地址不正确或不可达" << std::endl;
            std::cerr << "  2. RTMP服务器未运行或端口被防火墙阻止" << std::endl;
            std::cerr << "  3. RTMP URL格式错误（应为: rtmp://host:port/app/stream）" << std::endl;
            std::cerr << "  4. 网络连接问题" << std::endl;
            if (error_msg.find("Connection refused") != std::string::npos) {
                std::cerr << "  提示: 连接被拒绝，请检查RTMP服务器是否正在运行" << std::endl;
            } else if (error_msg.find("No route to host") != std::string::npos) {
                std::cerr << "  提示: 无法路由到主机，请检查网络连接和防火墙设置" << std::endl;
            } else if (error_msg.find("Connection timed out") != std::string::npos) {
                std::cerr << "  提示: 连接超时，请检查RTMP服务器地址和端口" << std::endl;
            }
            cleanupFFmpegPushStream(context);
            return false;
        }
        std::cout << "RTMP服务器连接成功" << std::endl;
    }
    
    // 设置FLV/RTMP选项
    AVDictionary* format_opts = nullptr;
    // 设置FLV标志：不写入duration和filesize（适用于实时流）
    av_dict_set(&format_opts, "flvflags", "no_duration_filesize", 0);
    // 设置RTMP选项：低延迟
    av_dict_set(&format_opts, "rtmp_live", "live", 0);
    // 设置缓冲区大小
    av_dict_set_int(&format_opts, "buffer_size", 131072, 0);  // 128KB
    // 设置RTMP推流超时（毫秒）
    av_dict_set_int(&format_opts, "rtmp_timeout", 5000, 0);
    // 设置RTMP应用名称（如果需要）
    // av_dict_set(&format_opts, "rtmp_app", "rtmp", 0);
    
    // 写入文件头
    ret = avformat_write_header(context->fmt_ctx, &format_opts);
    av_dict_free(&format_opts);
    if (ret < 0) {
        std::cerr << "无法写入文件头: " << avErrorToString(ret) << std::endl;
        cleanupFFmpegPushStream(context);
        return false;
    }
    
    // 初始化时间戳和开始时间
    context->pts = 0;
    context->push_start_time = std::chrono::steady_clock::now();
    context->first_frame_sent = false;
    return true;
}

/**
 * @brief 清理FFmpeg推流资源
 * @param context StreamContext指针
 */
void StreamManager::cleanupFFmpegPushStream(StreamContext* context) {
    if (!context) {
        return;
    }
    
    // 写入文件尾（如果连接仍然有效）
    if (context->fmt_ctx) {
        // 检查连接状态：如果 pb 存在且没有错误标志，尝试写入文件尾
        bool connection_valid = false;
        if (context->fmt_ctx->pb) {
            // 检查 IO 上下文是否有效（没有错误标志）
            if (!(context->fmt_ctx->pb->error)) {
                connection_valid = true;
            }
        }
        
        if (connection_valid) {
            int ret = av_write_trailer(context->fmt_ctx);
            if (ret < 0) {
                std::string error_msg = avErrorToString(ret);
                // 检查是否是连接相关的错误
                if (ret == AVERROR(EPIPE) || ret == AVERROR(ECONNRESET) || 
                    error_msg.find("Broken pipe") != std::string::npos ||
                    error_msg.find("Connection reset") != std::string::npos ||
                    error_msg.find("Connection refused") != std::string::npos) {
                    // 连接已断开，这是正常情况，不输出错误日志
                    std::cout << "RTMP推流连接已断开，正常关闭推流" << std::endl;
                } else {
                    // 其他错误，输出详细信息用于诊断
                    std::cerr << "写入文件尾失败: " << error_msg 
                              << " (错误码: " << ret << ")" << std::endl;
                }
            } else {
                std::cout << "RTMP推流文件尾写入成功" << std::endl;
            }
        } else {
            // 连接已无效，直接跳过写入文件尾
            std::cout << "RTMP推流连接已断开，跳过写入文件尾" << std::endl;
        }
    }
    
    // 关闭输出
    if (context->fmt_ctx && context->fmt_ctx->pb) {
        avio_closep(&context->fmt_ctx->pb);
    }
    
    // 释放颜色空间转换上下文
    if (context->sws_ctx) {
        sws_freeContext(context->sws_ctx);
        context->sws_ctx = nullptr;
    }
    
    // 释放帧
    if (context->frame) {
        av_frame_free(&context->frame);
    }
    if (context->frame_yuv) {
        av_frame_free(&context->frame_yuv);
    }
    if (context->pkt) {
        av_packet_free(&context->pkt);
    }
    
    // 释放编码器上下文
    if (context->codec_ctx) {
        avcodec_free_context(&context->codec_ctx);
    }
    
    // 释放格式上下文
    if (context->fmt_ctx) {
        avformat_free_context(context->fmt_ctx);
        context->fmt_ctx = nullptr;
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
            if (context->push_stream_enabled && context->fmt_ctx) {
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
    if (context_ref->push_stream_enabled && context_ref->fmt_ctx) {
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
        bool need_detection = (frame_counter % detection_interval == 0);
        
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
        if (context->push_stream_enabled && context->fmt_ctx && context->codec_ctx) {
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
            
            // 将OpenCV Mat转换为AVFrame并推流
            if (frame_to_push.cols == context->push_width && 
                frame_to_push.rows == context->push_height &&
                frame_to_push.type() == CV_8UC3) {
                
                // 确保frame是可写的
                if (av_frame_make_writable(context->frame) < 0) {
                    continue;
                }
                
                // 复制BGR数据到AVFrame
                const uint8_t* src_data[1] = {frame_to_push.data};
                int src_linesize[1] = {static_cast<int>(frame_to_push.step[0])};
                av_image_copy(context->frame->data, context->frame->linesize,
                             src_data, src_linesize,
                             AV_PIX_FMT_BGR24, context->push_width, context->push_height);
                
                // 转换BGR到YUV420P
                sws_scale(context->sws_ctx,
                          (const uint8_t* const*)context->frame->data, context->frame->linesize,
                          0, context->push_height,
                          context->frame_yuv->data, context->frame_yuv->linesize);
                
                // 计算基于实际时间的时间戳
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    current_time - context->push_start_time).count();
                // 将微秒转换为编码器时间基单位
                // time_base = {1, fps}，所以 pts = elapsed_us * fps / 1000000
                // 使用 framerate 的 num 和 den 来避免整数除法精度损失
                int64_t calculated_pts = (elapsed * context->codec_ctx->framerate.num) / 
                                        (1000000 * context->codec_ctx->framerate.den);
                // 确保时间戳单调递增（防止时间回退导致的问题）
                if (calculated_pts > context->pts) {
                    context->pts = calculated_pts;
                } else {
                    context->pts++;  // 如果计算出的时间戳小于等于当前值，则递增
                }
                context->frame_yuv->pts = context->pts;
                
                // 强制第一个帧为关键帧
                if (!context->first_frame_sent) {
                    context->frame_yuv->pict_type = AV_PICTURE_TYPE_I;
                }
                
                // 编码帧
                int ret = avcodec_send_frame(context->codec_ctx, context->frame_yuv);
                if (ret < 0) {
                    if (ret != AVERROR(EAGAIN)) {
                        std::cerr << "编码帧失败: " << avErrorToString(ret) << std::endl;
                    }
                } else {
                    // 接收编码后的数据包
                    while (ret >= 0) {
                        ret = avcodec_receive_packet(context->codec_ctx, context->pkt);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        } else if (ret < 0) {
                            std::cerr << "接收编码数据包失败: " << avErrorToString(ret) << std::endl;
                            break;
                        }
                        
                        // 设置流索引和时间戳
                        context->pkt->stream_index = 0;
                        av_packet_rescale_ts(context->pkt, context->codec_ctx->time_base, 
                                           context->fmt_ctx->streams[0]->time_base);
                        
                        // 确保 DTS 有效（如果 DTS 无效，使用 PTS）
                        if (context->pkt->dts == AV_NOPTS_VALUE) {
                            context->pkt->dts = context->pkt->pts;
                        }
                        // 确保 DTS <= PTS
                        if (context->pkt->dts > context->pkt->pts) {
                            context->pkt->dts = context->pkt->pts;
                        }
                        
                        // 写入数据包
                        ret = av_interleaved_write_frame(context->fmt_ctx, context->pkt);
                        if (ret < 0) {
                            std::string error_msg = avErrorToString(ret);
                            std::cerr << "通道 " << channel_id << " 写入数据包失败: " << error_msg 
                                      << " (错误码: " << ret << ", PTS: " << context->pkt->pts 
                                      << ", DTS: " << context->pkt->dts 
                                      << ", 关键帧: " << (context->pkt->flags & AV_PKT_FLAG_KEY ? "是" : "否") << ")" << std::endl;
                            
                            // 检查是否是连接断开错误（Broken pipe、Connection reset等）
                            bool is_connection_error = false;
                            if (ret == AVERROR(EPIPE) || errno == EPIPE || 
                                error_msg.find("Broken pipe") != std::string::npos ||
                                error_msg.find("broken pipe") != std::string::npos ||
                                ret == AVERROR(ECONNRESET) || errno == ECONNRESET ||
                                error_msg.find("Connection reset") != std::string::npos ||
                                error_msg.find("Connection refused") != std::string::npos) {
                                is_connection_error = true;
                            }
                            
                            if (is_connection_error) {
                                std::cerr << "通道 " << channel_id << " RTMP推流连接已断开，将尝试重连" << std::endl;
                                
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
                                
                                // 跳出内层循环，不再尝试写入数据包
                                break;
                            }
                        } else {
                            // 写入成功，标记第一帧已发送
                            if (!context->first_frame_sent) {
                                context->first_frame_sent = true;
                                std::cout << "通道 " << channel_id << " 第一个数据包写入成功 (PTS: " 
                                          << context->pkt->pts << ", 关键帧: " 
                                          << (context->pkt->flags & AV_PKT_FLAG_KEY ? "是" : "否") << ")" << std::endl;
                            }
                        }
                        
                        av_packet_unref(context->pkt);
                    }
                }
            }
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

