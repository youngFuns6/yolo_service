#pragma once

#include <string>
#include <vector>
#include <memory>

namespace detector_service {

struct AlertRecord {
    int id;
    int channel_id;
    std::string channel_name;
    std::string alert_type;
    std::string image_path;
    std::string image_data;  // base64 encoded
    float confidence;
    std::string detected_objects;  // JSON string
    std::string created_at;
    double bbox_x;
    double bbox_y;
    double bbox_w;
    double bbox_h;
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

    // 清理旧数据
    bool cleanupOldAlerts(int days);

private:
    AlertManager() = default;
    ~AlertManager() = default;
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;
};

} // namespace detector_service

