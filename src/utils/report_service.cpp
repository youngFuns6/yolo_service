#include "report_service.h"
#include "alert.h"
#include <iostream>
#include <sstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <mosquitto.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <ctime>

namespace detector_service {

// MQTT 连接回调函数
void ReportService::on_connect(struct mosquitto* mosq, void* obj, int rc) {
    (void)mosq;  // 未使用参数
    ReportService* service = static_cast<ReportService*>(obj);
    std::unique_lock<std::mutex> lock(service->mqtt_mutex_);
    
    if (rc == 0) {
        // 连接成功
        service->mqtt_connected_ = true;
        service->mqtt_connect_failed_ = false;
    } else {
        // 连接失败，销毁客户端
        service->mqtt_connected_ = false;
        service->mqtt_connect_failed_ = true;
        std::cerr << "MQTT 连接失败: " << mosquitto_connack_string(rc) 
                  << " (错误代码: " << rc << ")" << std::endl;
    }
    
    service->mqtt_connect_cv_.notify_one();
}

// MQTT 断开连接回调函数
void ReportService::on_disconnect(struct mosquitto* mosq, void* obj, int rc) {
    (void)mosq;  // 未使用参数
    (void)rc;    // 未使用参数
    ReportService* service = static_cast<ReportService*>(obj);
    std::unique_lock<std::mutex> lock(service->mqtt_mutex_);
    
    service->mqtt_connected_ = false;
}


bool ReportService::reportAlert(const AlertRecord& alert, const ReportConfig& config) {
    // 检查配置是否启用
    if (!config.enabled.load()) {
        return false;
    }
    
    // 将上报任务加入队列，异步处理，立即返回，不阻塞
    {
        std::lock_guard<std::mutex> lock(report_queue_mutex_);
        if (!report_worker_running_.load()) {
            return false;
        }
        
        ReportTask task;
        task.alert = alert;
        // 将 ReportConfig 转换为可复制的 ReportTaskConfig
        task.config.type = config.type;
        task.config.http_url = config.http_url;
        task.config.mqtt_broker = config.mqtt_broker;
        task.config.mqtt_port = config.mqtt_port;
        task.config.mqtt_topic = config.mqtt_topic;
        task.config.mqtt_username = config.mqtt_username;
        task.config.mqtt_password = config.mqtt_password;
        task.config.mqtt_client_id = config.mqtt_client_id;
        task.config.enabled = config.enabled.load();  // 读取 atomic 值
        report_queue_.push(task);
    }
    
    // 通知工作线程有新任务
    report_queue_cv_.notify_one();
    
    // 立即返回 true，表示任务已加入队列
    // 实际上报结果由后台线程处理
    return true;
}

std::string ReportService::buildAlertJson(const AlertRecord& alert) {
    nlohmann::json alert_json;
    alert_json["id"] = alert.id;
    alert_json["channel_id"] = alert.channel_id;
    alert_json["channel_name"] = alert.channel_name;
    alert_json["alert_type"] = alert.alert_type;
    alert_json["alert_rule_name"] = alert.alert_rule_name;
    alert_json["image_data"] = alert.image_data;
    alert_json["confidence"] = alert.confidence;
    alert_json["detected_objects"] = nlohmann::json::parse(alert.detected_objects.empty() ? "[]" : alert.detected_objects);
    alert_json["created_at"] = alert.created_at;
    
    return alert_json.dump();
}

bool ReportService::reportViaHttp(const AlertRecord& alert, const std::string& url) {
    if (url.empty()) {
        std::cerr << "上报失败: HTTP URL 为空" << std::endl;
        return false;
    }
    
    try {
        // 解析 URL
        std::string protocol, host, path;
        int port = 80;
        bool is_https = false;
        
        size_t protocol_end = url.find("://");
        if (protocol_end != std::string::npos) {
            protocol = url.substr(0, protocol_end);
            is_https = (protocol == "https");
            
            size_t host_start = protocol_end + 3;
            size_t path_start = url.find("/", host_start);
            size_t port_start = url.find(":", host_start);
            
            if (path_start == std::string::npos) {
                path_start = url.length();
            }
            
            if (port_start != std::string::npos && port_start < path_start) {
                host = url.substr(host_start, port_start - host_start);
                size_t port_end = (path_start < url.length()) ? path_start : url.length();
                port = std::stoi(url.substr(port_start + 1, port_end - port_start - 1));
            } else {
                host = url.substr(host_start, path_start - host_start);
                if (is_https) {
                    port = 443;
                }
            }
            
            if (path_start < url.length()) {
                path = url.substr(path_start);
            } else {
                path = "/";
            }
        } else {
            std::cerr << "上报失败: 无效的 URL 格式" << std::endl;
            return false;
        }
        
        // 构建 JSON 数据
        std::string json_str = buildAlertJson(alert);
        
        // 创建 HTTP 客户端
        httplib::Client client(host, port);
        client.set_connection_timeout(5, 0);  // 5秒连接超时
        client.set_read_timeout(5, 0);       // 5秒读取超时
        
        // 如果是 HTTPS，注意：cpp-httplib 需要编译时启用 SSL 支持
        // 如果 httplib 编译时未启用 SSL，HTTPS 请求将失败
        // 对于开发环境，如果需要禁用证书验证，可以在编译 httplib 时配置
        // 这里不设置证书验证相关选项，使用默认行为
        
        // 发送 POST 请求
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        
        auto res = client.Post(path.c_str(), headers, json_str, "application/json");
        
        if (!res) {
            std::cerr << "HTTP 上报失败: 无法连接到服务器" << std::endl;
            return false;
        }
        
        if (res->status != 200 && res->status != 201) {
            std::cerr << "HTTP 上报失败: 服务器返回状态码 " << res->status << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "HTTP 上报异常: " << e.what() << std::endl;
        return false;
    }
}

ReportService::ReportService() 
    : mqtt_client_(nullptr), current_port_(0), mqtt_initialized_(false),
      mqtt_connected_(false), mqtt_connect_failed_(false),
      last_reconnect_attempt_(std::chrono::steady_clock::now() - std::chrono::seconds(10)),
      report_worker_running_(true) {
    // 初始化 mosquitto 库
    mosquitto_lib_init();
    
    // 启动上报工作线程
    report_worker_thread_ = std::thread(&ReportService::reportWorker, this);
}

ReportService::~ReportService() {
    // 停止上报工作线程
    {
        std::lock_guard<std::mutex> lock(report_queue_mutex_);
        report_worker_running_ = false;
    }
    report_queue_cv_.notify_all();
    
    if (report_worker_thread_.joinable()) {
        report_worker_thread_.join();
    }
    
    cleanup();
    mosquitto_lib_cleanup();
}

void ReportService::cleanup() {
    std::lock_guard<std::mutex> lock(mqtt_mutex_);
    if (mqtt_client_ != nullptr) {
        // mosquitto_disconnect 可以安全地在未连接状态下调用
        mosquitto_disconnect(mqtt_client_);
        mosquitto_loop_stop(mqtt_client_, false);
        mosquitto_destroy(mqtt_client_);
        mqtt_client_ = nullptr;
    }
    mqtt_initialized_ = false;
    mqtt_connected_ = false;
    mqtt_connect_failed_ = false;
    current_broker_.clear();
    current_port_ = 0;
}

// 停止 MQTT 连接（当上报被禁用时调用）
void ReportService::stopMqttConnection() {
    // 清理 MQTT 客户端
    cleanup();
}

// 统一的 MQTT 客户端创建和连接方法
// 注意：调用此方法前必须已持有 mqtt_mutex_ 锁，并通过 lock 参数传入
// 注意：此方法会在调用 mosquitto_connect 前释放锁，避免阻塞
bool ReportService::createAndConnectMqttClient(const ReportConfig& config, std::unique_lock<std::mutex>& lock) {
    // 释放旧客户端
    if (mqtt_client_ != nullptr) {
        mosquitto_disconnect(mqtt_client_);
        mosquitto_loop_stop(mqtt_client_, false);
        mosquitto_destroy(mqtt_client_);
        mqtt_client_ = nullptr;
    }
    
    // 创建新客户端
    const char* client_id = config.mqtt_client_id.empty() ? 
        "detector_service" : config.mqtt_client_id.c_str();
    
    mqtt_client_ = mosquitto_new(client_id, true, this);
    if (mqtt_client_ == nullptr) {
        std::cerr << "MQTT 失败: 无法创建客户端" << std::endl;
        return false;
    }
    
    // 设置回调函数
    mosquitto_connect_callback_set(mqtt_client_, on_connect);
    mosquitto_disconnect_callback_set(mqtt_client_, on_disconnect);

    // 设置自动重连
    int reconnect_delay_s = 5;          // 初始重连延迟（秒）
    int max_reconnect_delay_s = 60;     // 最大重连延迟（秒）
    bool exponential_backoff = true;    // 指数退避

    mosquitto_reconnect_delay_set(mqtt_client_, reconnect_delay_s, max_reconnect_delay_s, exponential_backoff);
    
    // 设置用户名和密码
    if (!config.mqtt_username.empty() && !config.mqtt_password.empty()) {
        int rc = mosquitto_username_pw_set(mqtt_client_, 
                                            config.mqtt_username.c_str(),
                                            config.mqtt_password.c_str());
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "MQTT 失败: 设置用户名密码失败" << std::endl;
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
            return false;
        }
    }
    
    // 重置连接状态
    mqtt_connected_ = false;
    mqtt_connect_failed_ = false;
    
    // 启动网络循环（必须在连接之前启动）
    int rc = mosquitto_loop_start(mqtt_client_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT 失败: 启动网络循环失败 - " << mosquitto_strerror(rc) << std::endl;
        mosquitto_destroy(mqtt_client_);
        mqtt_client_ = nullptr;
        return false;
    }
    
    // 保存配置信息（在释放锁之前）
    current_broker_ = config.mqtt_broker;
    current_port_ = config.mqtt_port;
    mqtt_initialized_ = true;
    
    // 在调用 mosquitto_connect 之前释放锁，避免阻塞其他线程
    // mosquitto_connect 可能会进行DNS解析，这可能会阻塞
    lock.unlock();
    
    // 连接到 broker（异步连接，但可能进行DNS解析）
    // 验证 broker 地址格式（基本检查）
    if (config.mqtt_broker.empty()) {
        std::cerr << "MQTT 连接失败: broker 地址为空" << std::endl;
        lock.lock();
        if (mqtt_client_ != nullptr) {
            mosquitto_loop_stop(mqtt_client_, false);
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
        }
        lock.unlock();
        return false;
    }
    
    rc = mosquitto_connect(mqtt_client_, 
                           config.mqtt_broker.c_str(), 
                           config.mqtt_port, 
                           60); // keepalive 60秒
    
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT 连接失败: " << mosquitto_strerror(rc) 
                  << " (错误代码: " << rc << ")" << std::endl;
        std::cerr << "  broker: " << config.mqtt_broker 
                  << ", port: " << config.mqtt_port << std::endl;
        // 需要重新获取锁来清理资源
        lock.lock();
        if (mqtt_client_ != nullptr) {
            mosquitto_loop_stop(mqtt_client_, false);
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
        }
        lock.unlock();
        return false;
    }
    
    // 不等待连接结果，立即返回（连接结果通过回调处理）
    // 这样可以避免阻塞调用线程
    return true;
}

struct mosquitto* ReportService::getOrCreateMqttClient(const ReportConfig& config) {
    // 检查配置是否有效
    if (config.mqtt_broker.empty() || config.mqtt_topic.empty() || !config.enabled.load()) {
        // 如果上报被禁用或配置无效，确保停止现有连接
        if (mqtt_client_ != nullptr) {
            stopMqttConnection();
        }
        return nullptr;
    }
    
    std::unique_lock<std::mutex> lock(mqtt_mutex_);
    
    // 如果客户端未初始化，或者配置发生变化，则创建新客户端
    if (mqtt_client_ == nullptr || 
        !mqtt_initialized_ ||
        current_broker_ != config.mqtt_broker || 
        current_port_ != config.mqtt_port) {
        
        // 创建并连接客户端
        if (!createAndConnectMqttClient(config, lock)) {
            // 创建或连接请求失败，返回 nullptr
            return nullptr;
        }
        
        // createAndConnectMqttClient 内部会释放锁以进行非阻塞连接，
        // 但我们需要等待初始连接的结果。使用条件变量等待。
        // 在调用 wait_for 之前，锁必须被重新获取（如果之前被释放）。
        // createAndConnectMqttClient 保证在返回前不会重新获取锁。
        lock.lock();
        if (mqtt_connect_cv_.wait_for(lock, std::chrono::seconds(5), [this]{ return mqtt_connected_ || mqtt_connect_failed_; })) {
            if (mqtt_connected_) {
                return mqtt_client_;
            } else {
                std::cerr << "MQTT 客户端连接失败" << std::endl;
                return nullptr;
            }
        } else {
            std::cerr << "MQTT 连接超时" << std::endl;
            return nullptr;
        }
    }
    
    // 如果客户端已存在且配置未变，直接返回
    // mosquitto 库将在后台自动处理重连
    return mqtt_client_;
}

void ReportService::releaseMqttClient() {
    // 注意：此方法应在已持有 mqtt_mutex_ 锁的情况下调用
    if (mqtt_client_ != nullptr) {
        // mosquitto_disconnect 可以安全地在未连接状态下调用
        mosquitto_disconnect(mqtt_client_);
        mosquitto_loop_stop(mqtt_client_, false);
        mosquitto_destroy(mqtt_client_);
        mqtt_client_ = nullptr;
    }
    mqtt_initialized_ = false;
    mqtt_connected_ = false;
    mqtt_connect_failed_ = false;
    current_broker_.clear();
    current_port_ = 0;
}

bool ReportService::reportViaMqtt(const AlertRecord& alert, const ReportConfig& config) {
    if (config.mqtt_broker.empty() || config.mqtt_topic.empty()) {
        std::cerr << "MQTT 上报失败: broker 或 topic 为空" << std::endl;
        return false;
    }
    
    // 构建 JSON 数据
    std::string json_str = buildAlertJson(alert);
    
    // 获取或创建 MQTT 客户端
    // getOrCreateMqttClient 内部已经会等待连接建立（最多3秒），所以这里直接使用结果
    struct mosquitto* client = getOrCreateMqttClient(config);
    
    // 如果客户端仍然未连接，说明连接失败或超时
    if (client == nullptr) {
        std::cerr << "MQTT 客户端未连接，无法上报消息" << std::endl;
        std::cerr << "  请检查:" << std::endl;
        std::cerr << "  - MQTT broker 地址和端口是否正确: " << config.mqtt_broker << ":" << config.mqtt_port << std::endl;
        std::cerr << "  - 网络是否能够访问 broker" << std::endl;
        std::cerr << "  - broker 服务是否正在运行" << std::endl;
        std::cerr << "  - 用户名和密码是否正确（如果配置了）" << std::endl;
        std::cerr << "  下次上报时将自动尝试重新连接" << std::endl;
        return false;
    }
    
    // 发布消息
    int rc = mosquitto_publish(client, 
                               nullptr,  // mid (message id)
                               config.mqtt_topic.c_str(),
                               json_str.length(),
                               json_str.c_str(),
                               1,  // QoS 1
                               false);  // retain
    
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT 发布失败: " << mosquitto_strerror(rc) 
                  << " (错误代码: " << rc << ")" << std::endl;
        return false;
    }
    
    return true;
}


// 上报工作线程函数
void ReportService::reportWorker() {
    while (report_worker_running_.load()) {
        std::unique_lock<std::mutex> lock(report_queue_mutex_);
        
        // 等待队列中有任务或线程停止信号
        report_queue_cv_.wait(lock, [this] {
            return !report_queue_.empty() || !report_worker_running_.load();
        });
        
        if (!report_worker_running_.load()) {
            break;
        }
        
        // 处理队列中的所有任务
        while (!report_queue_.empty() && report_worker_running_.load()) {
            ReportTask task = report_queue_.front();
            report_queue_.pop();
            lock.unlock();
            
            // 执行实际上报（可能阻塞，但在后台线程中执行，不影响主线程）
            executeReport(task.alert, task.config);
            
            lock.lock();
        }
    }
}

// 实际执行上报（在工作线程中调用）
void ReportService::executeReport(const AlertRecord& alert, const ReportTaskConfig& task_config) {
    bool report_success = false;
    std::string report_url;
    
    try {
        // 检查配置是否仍然启用
        if (!task_config.enabled) {
            return;
        }
        
        // 将 ReportTaskConfig 转换为 ReportConfig 用于上报
        ReportConfig config;
        config.type = task_config.type;
        config.http_url = task_config.http_url;
        config.mqtt_broker = task_config.mqtt_broker;
        config.mqtt_port = task_config.mqtt_port;
        config.mqtt_topic = task_config.mqtt_topic;
        config.mqtt_username = task_config.mqtt_username;
        config.mqtt_password = task_config.mqtt_password;
        config.mqtt_client_id = task_config.mqtt_client_id;
        config.enabled.store(task_config.enabled);
        
        // 构建上报地址
        if (config.type == ReportType::HTTP) {
            report_url = config.http_url;
            report_success = reportViaHttp(alert, config.http_url);
        } else if (config.type == ReportType::MQTT) {
            report_url = config.mqtt_broker + ":" + std::to_string(config.mqtt_port) + "/" + config.mqtt_topic;
            report_success = reportViaMqtt(alert, config);
        }
        
        // 更新上报状态到数据库
        auto& alert_manager = AlertManager::getInstance();
        std::string report_status = report_success ? "success" : "failed";
        alert_manager.updateAlertReportStatus(alert.id, report_status, report_url);
        
    } catch (const std::exception& e) {
        std::cerr << "上报异常: " << e.what() << std::endl;
        // 更新为失败状态
        auto& alert_manager = AlertManager::getInstance();
        alert_manager.updateAlertReportStatus(alert.id, "failed", report_url);
    } catch (...) {
        std::cerr << "上报未知异常" << std::endl;
        // 更新为失败状态
        auto& alert_manager = AlertManager::getInstance();
        alert_manager.updateAlertReportStatus(alert.id, "failed", report_url);
    }
}

} // namespace detector_service

