#include "alert.h"
#include "database.h"
#include <iostream>
#include <sstream>

namespace detector_service {

int AlertManager::createAlert(const AlertRecord& alert) {
    auto& db = Database::getInstance();
    return db.insertAlert(alert);
}

bool AlertManager::deleteAlert(int alert_id) {
    auto& db = Database::getInstance();
    return db.deleteAlert(alert_id);
}

bool AlertManager::deleteAlertsByChannel(int channel_id) {
    auto& db = Database::getInstance();
    return db.deleteAlertsByChannel(channel_id);
}

std::vector<AlertRecord> AlertManager::getAlerts(int limit, int offset) {
    auto& db = Database::getInstance();
    return db.getAlerts(limit, offset);
}

std::vector<AlertRecord> AlertManager::getAlertsByChannel(int channel_id, int limit, int offset) {
    auto& db = Database::getInstance();
    return db.getAlertsByChannel(channel_id, limit, offset);
}

AlertRecord AlertManager::getAlert(int alert_id) {
    auto& db = Database::getInstance();
    return db.getAlert(alert_id);
}

int AlertManager::getAlertCount() {
    auto& db = Database::getInstance();
    return db.getAlertCount();
}

int AlertManager::getAlertCountByChannel(int channel_id) {
    auto& db = Database::getInstance();
    return db.getAlertCountByChannel(channel_id);
}

bool AlertManager::cleanupOldAlerts(int days) {
    auto& db = Database::getInstance();
    return db.cleanupOldAlerts(days);
}

bool AlertManager::isAlertSuppressed(int channel_id, int rule_id, int suppression_window_seconds) {
    std::lock_guard<std::mutex> lock(suppression_mutex_);
    
    std::ostringstream key_stream;
    key_stream << channel_id << ":" << rule_id;
    std::string key = key_stream.str();
    
    auto it = alert_suppression_map_.find(key);
    if (it == alert_suppression_map_.end()) {
        return false;  // 没有记录，不在抑制窗口内
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
    
    // 如果还在抑制窗口内，返回true（被抑制）
    return elapsed < suppression_window_seconds;
}

void AlertManager::recordAlertTrigger(int channel_id, int rule_id) {
    std::lock_guard<std::mutex> lock(suppression_mutex_);
    
    std::ostringstream key_stream;
    key_stream << channel_id << ":" << rule_id;
    std::string key = key_stream.str();
    
    // 记录当前时间
    alert_suppression_map_[key] = std::chrono::system_clock::now();
    
    // 清理过期的抑制记录（超过1小时的记录可以删除）
    auto now = std::chrono::system_clock::now();
    for (auto it = alert_suppression_map_.begin(); it != alert_suppression_map_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (elapsed > 3600) {  // 超过1小时
            it = alert_suppression_map_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace detector_service

