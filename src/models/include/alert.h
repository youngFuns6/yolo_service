#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <mutex>

namespace detector_service {

struct AlertRecord {
    int id;
    int channel_id;
    std::string channel_name;
    std::string alert_type;
    int alert_rule_id;           // 触发的告警规则ID
    std::string alert_rule_name;  // 触发的告警规则名称
    std::string image_path;
    std::string image_data;  // base64 encoded (不存储到数据库，仅用于上报)
    float confidence;
    std::string detected_objects;  // JSON string
    std::string created_at;
    double bbox_x;
    double bbox_y;
    double bbox_w;
    double bbox_h;
    std::string report_status;  // 上报状态: pending, success, failed
    std::string report_url;     // 上报地址
    
    AlertRecord() : id(0), channel_id(0), alert_rule_id(0), confidence(0.0f),
                    bbox_x(0), bbox_y(0), bbox_w(0), bbox_h(0) {}
};

class AlertManager {
public:
    static AlertManager& getInstance() {
        static AlertManager instance;
        return instance;
    }

    // 报警记录管理
    int createAlert(const AlertRecord& alert);
    bool deleteAlert(int alert_id);
    bool deleteAlertsByChannel(int channel_id);
    std::vector<AlertRecord> getAlerts(int limit = 100, int offset = 0);
    std::vector<AlertRecord> getAlertsByChannel(int channel_id, int limit = 100, int offset = 0);
    AlertRecord getAlert(int alert_id);
    int getAlertCount();
    int getAlertCountByChannel(int channel_id);
    bool updateAlertReportStatus(int alert_id, const std::string& report_status, const std::string& report_url);

    // 清理旧数据
    bool cleanupOldAlerts(int days);
    
    // 检查告警规则是否在抑制窗口内（防止重复告警）
    bool isAlertSuppressed(int channel_id, int rule_id, int suppression_window_seconds);
    
    // 记录告警触发时间（用于抑制机制）
    void recordAlertTrigger(int channel_id, int rule_id);

private:
    AlertManager() = default;
    ~AlertManager() = default;
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;
    
    // 告警抑制记录：key = "channel_id:rule_id", value = 最后触发时间戳
    std::map<std::string, std::chrono::system_clock::time_point> alert_suppression_map_;
    std::mutex suppression_mutex_;
};

} // namespace detector_service

