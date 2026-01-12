#include "report_service.h"
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <mosquitto.h>
#include <cstring>
#include <thread>
#include <chrono>

namespace detector_service {

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
    if (!config.enabled.load()) {
        return false;
    }
    
    if (config.type == ReportType::HTTP) {
        return reportViaHttp(alert, config.http_url);
    } else if (config.type == ReportType::MQTT) {
        return reportViaMqtt(alert, config);
    }
    
    return false;
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
    : mqtt_client_(nullptr), mqtt_initialized_(false), current_port_(0) {
    // 初始化 mosquitto 库
    mosquitto_lib_init();
}

ReportService::~ReportService() {
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
    current_broker_.clear();
    current_port_ = 0;
}

struct mosquitto* ReportService::getOrCreateMqttClient(const ReportConfig& config) {
    std::lock_guard<std::mutex> lock(mqtt_mutex_);
    
    // 如果配置改变或客户端不存在，需要重新创建
    if (mqtt_client_ == nullptr || 
        current_broker_ != config.mqtt_broker || 
        current_port_ != config.mqtt_port) {
        
        // 释放旧客户端（不需要加锁，因为已经在锁内）
        if (mqtt_client_ != nullptr) {
            // mosquitto_disconnect 可以安全地在未连接状态下调用
            mosquitto_disconnect(mqtt_client_);
            mosquitto_loop_stop(mqtt_client_, false);
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
        }
        
        // 创建新客户端
        const char* client_id = config.mqtt_client_id.empty() ? 
            "detector_service" : config.mqtt_client_id.c_str();
        
        mqtt_client_ = mosquitto_new(client_id, true, nullptr);
        if (mqtt_client_ == nullptr) {
            std::cerr << "MQTT 上报失败: 无法创建客户端" << std::endl;
            return nullptr;
        }
        
        // 设置用户名和密码
        if (!config.mqtt_username.empty() && !config.mqtt_password.empty()) {
            int rc = mosquitto_username_pw_set(mqtt_client_, 
                                                config.mqtt_username.c_str(),
                                                config.mqtt_password.c_str());
            if (rc != MOSQ_ERR_SUCCESS) {
                std::cerr << "MQTT 上报失败: 设置用户名密码失败" << std::endl;
                mosquitto_destroy(mqtt_client_);
                mqtt_client_ = nullptr;
                return nullptr;
            }
        }
        
        // 连接到 broker
        int rc = mosquitto_connect(mqtt_client_, 
                                   config.mqtt_broker.c_str(), 
                                   config.mqtt_port, 
                                   60); // keepalive 60秒
        
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "MQTT 上报失败: 连接失败 - " << mosquitto_strerror(rc) << std::endl;
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
            return nullptr;
        }
        
        // 等待连接完成
        rc = mosquitto_loop_start(mqtt_client_);
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "MQTT 上报失败: 启动网络循环失败" << std::endl;
            mosquitto_disconnect(mqtt_client_);
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
            return nullptr;
        }
        
        // 等待连接建立（使用 mosquitto_want_write 来检查连接状态）
        int retry = 0;
        bool connected = false;
        while (retry < 10 && !connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // mosquitto_want_write 返回 true 通常表示已连接且可以写入
            if (mosquitto_want_write(mqtt_client_)) {
                connected = true;
            }
            retry++;
        }
        
        if (!connected) {
            std::cerr << "MQTT 上报失败: 连接超时" << std::endl;
            mosquitto_disconnect(mqtt_client_);
            mosquitto_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
            return nullptr;
        }
        
        current_broker_ = config.mqtt_broker;
        current_port_ = config.mqtt_port;
        mqtt_initialized_ = true;
    }
    
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
    current_broker_.clear();
    current_port_ = 0;
}

bool ReportService::reportViaMqtt(const AlertRecord& alert, const ReportConfig& config) {
    if (config.mqtt_broker.empty() || config.mqtt_topic.empty()) {
        std::cerr << "上报失败: MQTT 配置不完整" << std::endl;
        return false;
    }
    
    // 构建 JSON 数据
    std::string json_str = buildAlertJson(alert);
    
    // 获取或创建 MQTT 客户端
    struct mosquitto* client = getOrCreateMqttClient(config);
    if (client == nullptr) {
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
        std::cerr << "MQTT 上报失败: 发布消息失败 - " << mosquitto_strerror(rc) << std::endl;
        // 如果发布失败，可能是连接断开，释放客户端以便下次重新连接
        std::lock_guard<std::mutex> lock(mqtt_mutex_);
        releaseMqttClient();
        return false;
    }
    
    std::cout << "MQTT 上报成功: 通道 " << alert.channel_id << " 报警已上报到主题 " << config.mqtt_topic << std::endl;
    return true;
}

} // namespace detector_service

