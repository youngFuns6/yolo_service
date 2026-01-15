#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include "report_config.h"
#include "alert.h"
#include <mosquitto.h>

namespace detector_service {

class ReportService {
public:
    static ReportService& getInstance() {
        static ReportService instance;
        return instance;
    }
    
    // 上报报警信息（异步非阻塞，立即返回）
    bool reportAlert(const AlertRecord& alert, const ReportConfig& config);
    
    // HTTP 上报（内部使用，可能阻塞）
    bool reportViaHttp(const AlertRecord& alert, const std::string& url);
    
    // MQTT 上报（内部使用，可能阻塞）
    bool reportViaMqtt(const AlertRecord& alert, const ReportConfig& config);
    
    // 清理资源
    void cleanup();
    
    // 停止 MQTT 连接（当上报被禁用时调用）
    void stopMqttConnection();

private:
    ReportService();
    ~ReportService();
    ReportService(const ReportService&) = delete;
    ReportService& operator=(const ReportService&) = delete;
    
    // 获取或创建 MQTT 客户端
    struct mosquitto* getOrCreateMqttClient(const ReportConfig& config);
    
    // 释放 MQTT 客户端
    void releaseMqttClient();
    
    // 统一的 MQTT 客户端创建和连接方法（内部使用，需要已持有 mqtt_mutex_ 锁）
    // lock: 已持有的 mqtt_mutex_ 锁的引用
    // 返回 true 表示成功，false 表示失败
    bool createAndConnectMqttClient(const ReportConfig& config, std::unique_lock<std::mutex>& lock);
    
    // 构建报警 JSON 数据
    std::string buildAlertJson(const AlertRecord& alert);
    
    // MQTT 连接回调函数（静态函数，用于传递给 mosquitto）
    static void on_connect(struct mosquitto* mosq, void* obj, int rc);
    static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);
    
    // MQTT 客户端句柄
    struct mosquitto* mqtt_client_;
    std::mutex mqtt_mutex_;
    std::condition_variable mqtt_connect_cv_;
    std::string current_broker_;
    int current_port_;
    bool mqtt_initialized_;
    bool mqtt_connected_;
    bool mqtt_connect_failed_;
    std::chrono::steady_clock::time_point last_reconnect_attempt_;  // 上次重连尝试时间
    
    // 异步上报任务结构（使用可复制的配置，避免 atomic 复制问题）
    struct ReportTaskConfig {
        ReportType type;
        std::string http_url;
        std::string mqtt_broker;
        int mqtt_port;
        std::string mqtt_topic;
        std::string mqtt_username;
        std::string mqtt_password;
        std::string mqtt_client_id;
        bool enabled;  // 使用普通 bool 而不是 atomic
        
        ReportTaskConfig() 
            : type(ReportType::HTTP),
              mqtt_port(1883),
              mqtt_client_id("detector_service"),
              enabled(false) {}
    };
    
    struct ReportTask {
        AlertRecord alert;
        ReportTaskConfig config;
    };
    
    // 异步上报队列和工作线程
    std::queue<ReportTask> report_queue_;
    std::mutex report_queue_mutex_;
    std::condition_variable report_queue_cv_;
    std::thread report_worker_thread_;
    std::atomic<bool> report_worker_running_;
    
    // 上报工作线程函数
    void reportWorker();
    
    // 实际执行上报（在工作线程中调用）
    void executeReport(const AlertRecord& alert, const ReportTaskConfig& task_config);
};

} // namespace detector_service

