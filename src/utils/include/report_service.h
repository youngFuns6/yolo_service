#pragma once

#include <string>
#include <memory>
#include <mutex>
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
    
    // 上报报警信息
    bool reportAlert(const AlertRecord& alert, const ReportConfig& config);
    
    // HTTP 上报
    bool reportViaHttp(const AlertRecord& alert, const std::string& url);
    
    // MQTT 上报
    bool reportViaMqtt(const AlertRecord& alert, const ReportConfig& config);
    
    // 清理资源
    void cleanup();

private:
    ReportService();
    ~ReportService();
    ReportService(const ReportService&) = delete;
    ReportService& operator=(const ReportService&) = delete;
    
    // 获取或创建 MQTT 客户端
    struct mosquitto* getOrCreateMqttClient(const ReportConfig& config);
    
    // 释放 MQTT 客户端
    void releaseMqttClient();
    
    // 构建报警 JSON 数据
    std::string buildAlertJson(const AlertRecord& alert);
    
    // MQTT 客户端句柄
    struct mosquitto* mqtt_client_;
    std::mutex mqtt_mutex_;
    std::string current_broker_;
    int current_port_;
    bool mqtt_initialized_;
};

} // namespace detector_service

