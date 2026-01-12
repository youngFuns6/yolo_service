#include "report_config.h"
#include "database.h"
#include <mutex>

namespace detector_service {

const ReportConfig& ReportConfigManager::getReportConfig() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // 从数据库加载配置
    auto& db = Database::getInstance();
    ReportConfig db_config;
    if (db.loadReportConfig(db_config)) {
        // 逐个字段赋值，因为 atomic 不能直接复制
        config_.type = db_config.type;
        config_.http_url = db_config.http_url;
        config_.mqtt_broker = db_config.mqtt_broker;
        config_.mqtt_port = db_config.mqtt_port;
        config_.mqtt_topic = db_config.mqtt_topic;
        config_.mqtt_username = db_config.mqtt_username;
        config_.mqtt_password = db_config.mqtt_password;
        config_.mqtt_client_id = db_config.mqtt_client_id;
        config_.enabled.store(db_config.enabled.load());
    }
    
    // 返回引用，避免复制 atomic
    return config_;
}

bool ReportConfigManager::updateReportConfig(const ReportConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // 逐个字段赋值，因为 atomic 不能直接复制
    config_.type = config.type;
    config_.http_url = config.http_url;
    config_.mqtt_broker = config.mqtt_broker;
    config_.mqtt_port = config.mqtt_port;
    config_.mqtt_topic = config.mqtt_topic;
    config_.mqtt_username = config.mqtt_username;
    config_.mqtt_password = config.mqtt_password;
    config_.mqtt_client_id = config.mqtt_client_id;
    config_.enabled.store(config.enabled.load());
    
    // 保存到数据库
    auto& db = Database::getInstance();
    return db.saveReportConfig(config);
}

bool ReportConfigManager::isReportEnabled() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.enabled.load();
}

} // namespace detector_service

