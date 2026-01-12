#pragma once

#include <string>
#include <atomic>
#include <mutex>

namespace detector_service {

enum class ReportType {
    HTTP,
    MQTT
};

struct ReportConfig {
    ReportType type;
    std::string http_url;           // HTTP 上报地址
    std::string mqtt_broker;        // MQTT broker 地址
    int mqtt_port;                  // MQTT 端口
    std::string mqtt_topic;         // MQTT 主题
    std::string mqtt_username;      // MQTT 用户名（可选）
    std::string mqtt_password;      // MQTT 密码（可选）
    std::string mqtt_client_id;     // MQTT 客户端 ID
    std::atomic<bool> enabled;      // 是否启用上报
    
    ReportConfig() 
        : type(ReportType::HTTP),
          mqtt_port(1883),
          enabled(false),
          mqtt_client_id("detector_service") {}
};

class ReportConfigManager {
public:
    static ReportConfigManager& getInstance() {
        static ReportConfigManager instance;
        return instance;
    }
    
    // 获取上报配置（返回引用以避免复制 atomic）
    const ReportConfig& getReportConfig();
    
    // 更新上报配置
    bool updateReportConfig(const ReportConfig& config);
    
    // 检查是否启用上报
    bool isReportEnabled();

private:
    ReportConfigManager() = default;
    ~ReportConfigManager() = default;
    ReportConfigManager(const ReportConfigManager&) = delete;
    ReportConfigManager& operator=(const ReportConfigManager&) = delete;
    
    ReportConfig config_;
    std::mutex config_mutex_;
};

} // namespace detector_service

