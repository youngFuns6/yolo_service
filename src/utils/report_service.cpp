#include "report_service.h"
#include "alert.h"
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <mosquitto.h>
#include <cstring>
#include <thread>
#include <chrono>

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
        std::cout << "MQTT 连接成功" << std::endl;
        
        // 连接成功时清除重连标志（如果重连线程正在运行）
        {
            std::lock_guard<std::mutex> reconnect_lock(service->reconnect_mutex_);
            service->should_reconnect_ = false;
        }
    } else {
        // 连接失败
        service->mqtt_connected_ = false;
        service->mqtt_connect_failed_ = true;
        std::cerr << "MQTT 连接失败: " << mosquitto_connack_string(rc) 
                  << " (错误代码: " << rc << ")" << std::endl;
        
        // 检查是否已经在重连中
        {
            std::lock_guard<std::mutex> reconnect_lock(service->reconnect_mutex_);
            if (service->should_reconnect_.load()) {
                std::cout << "MQTT 已在重连中，跳过重复触发" << std::endl;
                lock.unlock();
                return;
            }
            service->should_reconnect_ = true;
        }
        lock.unlock();
        
        // 在新线程中执行重连，避免阻塞回调
        std::thread([service]() {
            service->performReconnect();
        }).detach();
    }
    
    service->mqtt_connect_cv_.notify_one();
}

// MQTT 断开连接回调函数
void ReportService::on_disconnect(struct mosquitto* mosq, void* obj, int rc) {
    (void)mosq;  // 未使用参数
    ReportService* service = static_cast<ReportService*>(obj);
    std::unique_lock<std::mutex> lock(service->mqtt_mutex_);
    
    service->mqtt_connected_ = false;
    lock.unlock();
    
    if (rc != 0) {
        // 非正常断开，启动重连线程（避免阻塞回调）
        
        // 检查是否已经在重连中
        {
            std::lock_guard<std::mutex> reconnect_lock(service->reconnect_mutex_);
            if (service->should_reconnect_.load()) {
                return;
            }
            service->should_reconnect_ = true;
        }
        
        // 在新线程中执行重连，避免阻塞回调
        std::thread([service]() {
            service->performReconnect();
        }).detach();
    } else {
        std::cout << "MQTT 正常断开连接" << std::endl;
    }
}

// HTTP 回调函数，用于接收响应
struct HttpResponse {
    std::string data;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    HttpResponse* response = static_cast<HttpResponse*>(userp);
    response->data.append(static_cast<char*>(contents), total_size);
    return total_size;
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
    alert_json["image_data"] = "";
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
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "上报失败: 无法初始化 CURL" << std::endl;
        return false;
    }
    
    // 构建 JSON 数据
    std::string json_str = buildAlertJson(alert);
    
    HttpResponse response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_str.length());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // 设置 HTTP 头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // 设置超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "HTTP 上报失败: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    std::cout << "HTTP 上报成功: 通道 " << alert.channel_id << " 报警已上报" << std::endl;
    return true;
}

ReportService::ReportService() 
    : mqtt_client_(nullptr), current_port_(0), mqtt_initialized_(false),
      mqtt_connected_(false), mqtt_connect_failed_(false),
      should_reconnect_(false), reconnect_thread_running_(true),
      report_worker_running_(true) {
    // 初始化 mosquitto 库
    mosquitto_lib_init();
    
    // 启动重连线程
    reconnect_thread_ = std::thread(&ReportService::reconnectWorker, this);
    
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
    
    // 停止重连线程
    {
        std::lock_guard<std::mutex> lock(reconnect_mutex_);
        reconnect_thread_running_ = false;
        should_reconnect_ = false;
    }
    reconnect_cv_.notify_all();
    
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
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

// 停止 MQTT 连接和重连（当上报被禁用时调用）
void ReportService::stopMqttConnection() {
    std::cout << "MQTT 停止连接和重连..." << std::endl;
    
    // 停止重连
    {
        std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
        should_reconnect_ = false;
        reconnect_config_.enabled = false;  // 标记配置为禁用
    }
    reconnect_cv_.notify_all();  // 唤醒重连线程，让它检查配置并退出
    
    // 清理 MQTT 客户端
    cleanup();
    
    std::cout << "MQTT 连接和重连已停止" << std::endl;
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
    std::cout << "MQTT 正在连接到 " << config.mqtt_broker << ":" << config.mqtt_port 
              << " (client_id: " << client_id << ")..." << std::endl;
    
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
    
    std::cout << "MQTT 连接请求已发送，等待连接结果..." << std::endl;
    // 不等待连接结果，立即返回（连接结果通过回调处理）
    // 这样可以避免阻塞调用线程
    return true;
}

struct mosquitto* ReportService::getOrCreateMqttClient(const ReportConfig& config) {
    // 检查配置是否有效
    if (config.mqtt_broker.empty() || config.mqtt_topic.empty() || !config.enabled.load()) {
        return nullptr;
    }
    
    // 快速检查连接状态，不持有锁过久
    {
        std::lock_guard<std::mutex> lock(mqtt_mutex_);
        // 如果已经连接且配置未改变，直接返回现有客户端
        if (mqtt_client_ != nullptr && 
            mqtt_connected_ && 
            current_broker_ == config.mqtt_broker && 
            current_port_ == config.mqtt_port) {
            return mqtt_client_;
        }
    }
    
    // 客户端未连接或不存在，立即尝试创建和连接（首次连接，不使用重连延迟）
    std::unique_lock<std::mutex> lock(mqtt_mutex_);
    
    // 再次检查是否在获取锁的过程中已经连接成功
    if (mqtt_client_ != nullptr && 
        mqtt_connected_ && 
        current_broker_ == config.mqtt_broker && 
        current_port_ == config.mqtt_port) {
        return mqtt_client_;
    }
    
    // 保存配置以便重连时使用
    {
        std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
        reconnect_config_ = createReconnectConfig(config);
    }
    
    // 立即尝试创建和连接（首次连接，不使用重连延迟）
    std::cout << "MQTT 首次连接或配置变更，立即尝试连接..." << std::endl;
    bool result = createAndConnectMqttClient(config, lock);
    // 注意：createAndConnectMqttClient 内部已经释放了锁
    
    if (result) {
        // 连接请求已发送，等待连接结果（最多等待3秒）
        std::cout << "MQTT 连接请求已发送，等待连接结果..." << std::endl;
        for (int i = 0; i < 30; ++i) {  // 30 * 100ms = 3秒
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            {
                std::lock_guard<std::mutex> check_lock(mqtt_mutex_);
                if (mqtt_connected_ && mqtt_client_ != nullptr &&
                    current_broker_ == config.mqtt_broker && 
                    current_port_ == config.mqtt_port) {
                    std::cout << "MQTT 连接成功" << std::endl;
                    return mqtt_client_;
                }
                if (mqtt_connect_failed_) {
                    std::cerr << "MQTT 连接失败，将触发重连线程..." << std::endl;
                    // 连接失败，触发重连线程（使用重连延迟）
                    {
                        std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                        if (!should_reconnect_.load()) {
                            should_reconnect_ = true;
                            std::thread([this]() {
                                performReconnect();
                            }).detach();
                        }
                    }
                    return nullptr;
                }
            }
        }
        // 连接超时，触发重连线程
        std::cerr << "MQTT 连接超时，将触发重连线程..." << std::endl;
        {
            std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
            if (!should_reconnect_.load()) {
                should_reconnect_ = true;
                std::thread([this]() {
                    performReconnect();
                }).detach();
            }
        }
        return nullptr;
    } else {
        // 创建失败，触发重连线程
        std::cerr << "MQTT 客户端创建失败，将触发重连线程..." << std::endl;
        {
            std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
            if (!should_reconnect_.load()) {
                should_reconnect_ = true;
                std::thread([this]() {
                    performReconnect();
                }).detach();
            }
        }
        return nullptr;
    }
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
    // getOrCreateMqttClient 已经触发了重连线程，这里直接返回失败
    if (client == nullptr) {
        std::cerr << "MQTT 客户端未连接，无法上报消息" << std::endl;
        std::cerr << "  如果这是首次连接，请检查:" << std::endl;
        std::cerr << "  - MQTT broker 地址和端口是否正确: " << config.mqtt_broker << ":" << config.mqtt_port << std::endl;
        std::cerr << "  - 网络是否能够访问 broker" << std::endl;
        std::cerr << "  - broker 服务是否正在运行" << std::endl;
        std::cerr << "  - 用户名和密码是否正确（如果配置了）" << std::endl;
        std::cerr << "  重连线程已启动，将在后台持续尝试重连" << std::endl;
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
        // std::cerr << "MQTT 发布失败: " << mosquitto_strerror(rc) 
        //           << " (错误代码: " << rc << ")" << std::endl;
        // 如果发布失败，可能是连接断开，释放客户端并触发重连
        std::unique_lock<std::mutex> lock(mqtt_mutex_);
        releaseMqttClient();
        lock.unlock();
        // 如果配置有效，直接触发重连（不依赖 current_broker_）
        if (!config.mqtt_broker.empty() && !config.mqtt_topic.empty() && config.enabled.load()) {
            triggerReconnect(config);
        }
        return false;
    }
    
    // std::cout << "MQTT 消息发布成功: topic=" << config.mqtt_topic << std::endl;
    return true;
}

// 重连线程工作函数
void ReportService::reconnectWorker() {
    std::cout << "MQTT 重连线程已启动" << std::endl;
    while (reconnect_thread_running_.load()) {
        std::unique_lock<std::mutex> lock(reconnect_mutex_);
        
        // 等待重连信号或线程停止信号
        reconnect_cv_.wait(lock, [this] {
            return should_reconnect_.load() || !reconnect_thread_running_.load();
        });
        
        if (!reconnect_thread_running_.load()) {
            break;
        }
        
        // 确保 should_reconnect_ 为 true
        if (!should_reconnect_.load()) {
            continue;
        }
        
        std::cout << "MQTT 重连线程被唤醒，开始重连流程" << std::endl;
        ReconnectConfig config = reconnect_config_;
        lock.unlock();
        
        int attempt = 0;
        // 持续重连直到连接成功、配置无效或线程停止
        while (reconnect_thread_running_.load()) {
            // 检查配置是否仍然有效（在每次循环开始时检查）
            bool config_valid = false;
            {
                std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                config = reconnect_config_;
                config_valid = (!config.mqtt_broker.empty() && 
                               !config.mqtt_topic.empty() && 
                               config.enabled);
                // 如果配置无效或被禁用，清除重连标志并退出
                if (!config_valid) {
                    should_reconnect_ = false;
                    std::cout << "MQTT 重连配置无效或已禁用，停止重连" << std::endl;
                    std::cout << "  broker: " << config.mqtt_broker << std::endl;
                    std::cout << "  topic: " << config.mqtt_topic << std::endl;
                    std::cout << "  enabled: " << config.enabled << std::endl;
                    break;
                }
                // 如果 should_reconnect_ 被清除但配置有效，说明可能是外部触发了停止
                // 这种情况下也应该退出
                if (!should_reconnect_.load()) {
                    std::cout << "MQTT 重连标志被清除，停止重连" << std::endl;
                    break;
                }
            }
            
            std::cout << "MQTT 重连循环迭代开始，attempt=" << attempt << std::endl;
            
            std::cout << "MQTT 配置检查完成，config_valid=" << config_valid << std::endl;
            
            // 检查是否已经连接成功
            {
                std::lock_guard<std::mutex> mqtt_lock(mqtt_mutex_);
                if (mqtt_connected_) {
                    std::cout << "MQTT 已连接，停止重连" << std::endl;
                    std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                    should_reconnect_ = false;
                    break;
                }
            }
            
            // 检查是否达到最大重试次数
            if (MAX_RECONNECT_ATTEMPTS > 0 && attempt >= MAX_RECONNECT_ATTEMPTS) {
                std::cerr << "MQTT 重连达到最大尝试次数 (" << MAX_RECONNECT_ATTEMPTS << ")，停止重连" << std::endl;
                std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                should_reconnect_ = false;
                break;
            }
            
            attempt++;
            std::cout << "尝试 MQTT 重连 (第 " << attempt << " 次)..." << std::endl;
            
            bool reconnect_result = false;
            try {
                reconnect_result = attemptReconnect(config);
            } catch (const std::exception& e) {
                std::cerr << "MQTT 重连异常: " << e.what() << std::endl;
                reconnect_result = false;
            } catch (...) {
                std::cerr << "MQTT 重连未知异常" << std::endl;
                reconnect_result = false;
            }
            
            if (reconnect_result) {
                std::cout << "MQTT 重连成功" << std::endl;
                std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                should_reconnect_ = false;
                break;
            }
            
            std::cerr << "MQTT 重连失败，等待 " << RECONNECT_INTERVAL_SECONDS 
                      << " 秒后重试..." << std::endl;
            
            // 等待重连间隔
            std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_INTERVAL_SECONDS));
            
            std::cout << "等待完成，准备下一次重连尝试" << std::endl;
        }
    }
}

// 从 ReportConfig 创建 ReconnectConfig
ReportService::ReconnectConfig ReportService::createReconnectConfig(const ReportConfig& config) {
    ReconnectConfig reconnect_config;
    reconnect_config.mqtt_broker = config.mqtt_broker;
    reconnect_config.mqtt_port = config.mqtt_port;
    reconnect_config.mqtt_topic = config.mqtt_topic;
    reconnect_config.mqtt_username = config.mqtt_username;
    reconnect_config.mqtt_password = config.mqtt_password;
    reconnect_config.mqtt_client_id = config.mqtt_client_id;
    reconnect_config.enabled = config.enabled.load();
    return reconnect_config;
}

// 触发重连
void ReportService::triggerReconnect(const ReportConfig& config) {
    // 只有在启用上报且配置有效时才触发重连
    if (!config.enabled.load() || config.mqtt_broker.empty() || config.mqtt_topic.empty()) {
        std::cerr << "MQTT 重连触发失败: 配置无效或未启用" << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(reconnect_mutex_);
    reconnect_config_ = createReconnectConfig(config);
    should_reconnect_ = true;
    std::cout << "MQTT 触发重连: broker=" << config.mqtt_broker 
              << ", port=" << config.mqtt_port << std::endl;
    reconnect_cv_.notify_one();
}

// 执行重连（使用指数退避策略，在新线程中调用）
void ReportService::performReconnect() {
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 10;  // 最大重连次数
    
    while (reconnect_thread_running_.load() && reconnect_attempts < max_reconnect_attempts) {
        // 检查是否已经连接
        {
            std::lock_guard<std::mutex> lock(mqtt_mutex_);
            if (mqtt_connected_ && mqtt_client_ != nullptr) {
                std::cout << "MQTT 已连接，停止重连" << std::endl;
                std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                should_reconnect_ = false;
                return;
            }
        }
        
        // 检查配置是否有效（在延迟之前检查，以便在日志中显示）
        ReconnectConfig config;
        {
            std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
            config = reconnect_config_;
            if (config.mqtt_broker.empty() || config.mqtt_topic.empty() || !config.enabled) {
                std::cerr << "MQTT 重连配置无效，停止重连" << std::endl;
                should_reconnect_ = false;
                return;
            }
        }
        
        // 计算延迟时间（指数退避）
        int delay_ms = (1 << reconnect_attempts) * 1000;  // 2^attempts 秒
        if (delay_ms > 60000) delay_ms = 60000;  // 最大延迟60秒
        
        std::cout << "尝试 MQTT 重连 (" << reconnect_attempts + 1 
                  << "/" << max_reconnect_attempts 
                  << ")，等待 " << delay_ms/1000 << " 秒..." << std::endl;
        std::cout << "  目标: " << config.mqtt_broker << ":" << config.mqtt_port 
                  << ", topic: " << config.mqtt_topic << std::endl;
        
        // 等待延迟时间（分段等待，以便快速响应配置变化）
        const int check_interval_ms = 500;  // 每500ms检查一次
        int waited_ms = 0;
        while (waited_ms < delay_ms && reconnect_thread_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
            waited_ms += check_interval_ms;
            
            // 检查配置是否仍然有效
            {
                std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                if (!reconnect_config_.enabled || 
                    reconnect_config_.mqtt_broker.empty() || 
                    reconnect_config_.mqtt_topic.empty()) {
                    std::cout << "MQTT 配置已禁用，停止重连" << std::endl;
                    should_reconnect_ = false;
                    return;
                }
            }
        }
        
        // 再次检查配置（在重连前）
        {
            std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
            if (!reconnect_config_.enabled || 
                reconnect_config_.mqtt_broker.empty() || 
                reconnect_config_.mqtt_topic.empty()) {
                std::cout << "MQTT 配置已禁用，停止重连" << std::endl;
                should_reconnect_ = false;
                return;
            }
            config = reconnect_config_;  // 更新配置
        }
        
        // 尝试重连
        std::unique_lock<std::mutex> lock(mqtt_mutex_);
        
        // 如果客户端存在，尝试使用 mosquitto_reconnect
        if (mqtt_client_ != nullptr) {
            std::cout << "MQTT 尝试使用现有客户端重连..." << std::endl;
            int rc = mosquitto_reconnect(mqtt_client_);
            if (rc == MOSQ_ERR_SUCCESS) {
                std::cout << "MQTT 重连请求已发送，等待连接结果..." << std::endl;
                reconnect_attempts = 0;
                lock.unlock();
                
                // 等待连接结果，最多等待 10 秒
                bool connected = false;
                for (int i = 0; i < 20; ++i) {  // 20 * 500ms = 10秒
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    {
                        std::lock_guard<std::mutex> check_lock(mqtt_mutex_);
                        if (mqtt_connected_) {
                            std::cout << "MQTT 重连成功" << std::endl;
                            connected = true;
                            break;
                        }
                        if (mqtt_connect_failed_) {
                            std::cerr << "MQTT 重连失败，连接被拒绝" << std::endl;
                            break;
                        }
                    }
                }
                
                if (connected) {
                    std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                    should_reconnect_ = false;
                    return;
                } else {
                    // 重连失败，清理客户端以便重新创建
                    std::lock_guard<std::mutex> cleanup_lock(mqtt_mutex_);
                    if (mqtt_client_ != nullptr) {
                        std::cout << "MQTT 重连超时或失败，清理客户端以便重新创建..." << std::endl;
                        mosquitto_disconnect(mqtt_client_);
                        mosquitto_loop_stop(mqtt_client_, false);
                        mosquitto_destroy(mqtt_client_);
                        mqtt_client_ = nullptr;
                    }
                }
            } else {
                std::cerr << "MQTT 重连失败: " << mosquitto_strerror(rc) 
                          << " (错误代码: " << rc << ")" << std::endl;
                // 如果 reconnect 失败，清理客户端以便重新创建
                if (mqtt_client_ != nullptr) {
                    std::cout << "MQTT 清理失败的客户端，准备重新创建..." << std::endl;
                    mosquitto_disconnect(mqtt_client_);
                    mosquitto_loop_stop(mqtt_client_, false);
                    mosquitto_destroy(mqtt_client_);
                    mqtt_client_ = nullptr;
                }
                lock.unlock();
            }
        } else {
            // 客户端不存在，需要重新创建
            lock.unlock();
            ReportConfig report_config;
            report_config.mqtt_broker = config.mqtt_broker;
            report_config.mqtt_port = config.mqtt_port;
            report_config.mqtt_topic = config.mqtt_topic;
            report_config.mqtt_username = config.mqtt_username;
            report_config.mqtt_password = config.mqtt_password;
            report_config.mqtt_client_id = config.mqtt_client_id;
            report_config.enabled.store(config.enabled);
            
            // createAndConnectMqttClient 内部会释放锁，所以这里需要重新获取锁
            lock.lock();
            std::cout << "MQTT 重新创建客户端并连接..." << std::endl;
            bool result = createAndConnectMqttClient(report_config, lock);
            // 注意：createAndConnectMqttClient 内部已经释放了锁，所以这里不需要再次解锁
            
            if (result) {
                std::cout << "MQTT 客户端重新创建成功，等待连接结果..." << std::endl;
                reconnect_attempts = 0;
                
                // 等待连接结果，最多等待 10 秒
                bool connected = false;
                for (int i = 0; i < 20; ++i) {  // 20 * 500ms = 10秒
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    {
                        std::lock_guard<std::mutex> check_lock(mqtt_mutex_);
                        if (mqtt_connected_) {
                            std::cout << "MQTT 重连成功" << std::endl;
                            connected = true;
                            break;
                        }
                        if (mqtt_connect_failed_) {
                            std::cerr << "MQTT 连接失败: broker=" << config.mqtt_broker 
                                      << ", port=" << config.mqtt_port << std::endl;
                            break;
                        }
                    }
                }
                
                if (connected) {
                    std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
                    should_reconnect_ = false;
                    return;
                } else {
                    std::cerr << "MQTT 连接超时或失败，将在下次重试时重新尝试" << std::endl;
                }
            } else {
                std::cerr << "MQTT 客户端创建失败，将在下次重试时重新尝试" << std::endl;
                // 如果创建失败，锁已经被 createAndConnectMqttClient 释放了
                // 不需要再次解锁
            }
        }
        
        reconnect_attempts++;
    }
    
    if (reconnect_attempts >= max_reconnect_attempts) {
        std::cerr << "MQTT 达到最大重连次数 (" << max_reconnect_attempts << ")，停止重试" << std::endl;
    }
    
    std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
    should_reconnect_ = false;
}

// 执行重连
bool ReportService::attemptReconnect(const ReconnectConfig& config) {
    std::unique_lock<std::mutex> lock(mqtt_mutex_);
    
    // 如果已经连接，不需要重连
    if (mqtt_connected_ && mqtt_client_ != nullptr) {
        std::cout << "MQTT 已连接，无需重连" << std::endl;
        return true;
    }
    
    // 将 ReconnectConfig 转换为 ReportConfig 以复用统一的创建逻辑
    ReportConfig report_config;
    report_config.mqtt_broker = config.mqtt_broker;
    report_config.mqtt_port = config.mqtt_port;
    report_config.mqtt_topic = config.mqtt_topic;
    report_config.mqtt_username = config.mqtt_username;
    report_config.mqtt_password = config.mqtt_password;
    report_config.mqtt_client_id = config.mqtt_client_id;
    report_config.enabled.store(config.enabled);
    
    std::cout << "MQTT 开始创建和连接客户端..." << std::endl;
    std::cout.flush();
    // 使用统一的创建和连接方法
    // 注意：createAndConnectMqttClient 内部会释放锁
    bool result = createAndConnectMqttClient(report_config, lock);
    std::cerr << "MQTT createAndConnectMqttClient 返回: " << (result ? "true" : "false") << std::endl;
    std::cerr.flush();
    
    if (!result) {
        std::cerr << "MQTT attemptReconnect: 连接失败，返回 false" << std::endl;
        std::cerr.flush();
        // 锁已经被 createAndConnectMqttClient 释放了，不需要再次解锁
        return false;
    }
    
    std::cout << "MQTT createAndConnectMqttClient 成功" << std::endl;
    std::cout.flush();
    // 锁已经被 createAndConnectMqttClient 释放了，不需要再次解锁
    return true;
}

// 上报工作线程函数
void ReportService::reportWorker() {
    std::cout << "上报工作线程已启动" << std::endl;
    
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
    
    std::cout << "上报工作线程已停止" << std::endl;
}

// 实际执行上报（在工作线程中调用）
void ReportService::executeReport(const AlertRecord& alert, const ReportTaskConfig& task_config) {
    bool report_success = false;
    std::string report_url;
    
    try {
        // 检查配置是否仍然启用
        if (!task_config.enabled) {
            std::cout << "上报已禁用，跳过上报任务" << std::endl;
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

