#include "alert.h"
#include "database.h"
#include <iostream>

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

} // namespace detector_service

