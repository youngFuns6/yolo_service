#include "gb28181_config.h"
#include "database.h"
#include <iostream>

namespace detector_service {

bool GB28181ConfigManager::saveGB28181Config(const GB28181Config& config) {
    auto& db = Database::getInstance();
    return db.saveGB28181Config(config);
}

bool GB28181ConfigManager::loadGB28181Config(GB28181Config& config) {
    auto& db = Database::getInstance();
    return db.loadGB28181Config(config);
}

GB28181Config GB28181ConfigManager::getGB28181Config() {
    GB28181Config config;
    if (!loadGB28181Config(config)) {
        // 如果加载失败，返回默认值
        config = GB28181Config();
    }
    return config;
}

} // namespace detector_service

