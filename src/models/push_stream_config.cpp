#include "push_stream_config.h"
#include "database.h"
#include <iostream>

namespace detector_service {

bool PushStreamConfigManager::savePushStreamConfig(const PushStreamConfig& config) {
    auto& db = Database::getInstance();
    return db.savePushStreamConfig(config.rtmp_url, config.width, config.height, config.fps, config.bitrate);
}

bool PushStreamConfigManager::loadPushStreamConfig(PushStreamConfig& config) {
    auto& db = Database::getInstance();
    
    std::string rtmp_url;
    std::optional<int> width, height, fps, bitrate;
    
    if (!db.loadPushStreamConfig(rtmp_url, width, height, fps, bitrate)) {
        return false;
    }
    
    config.rtmp_url = rtmp_url;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.bitrate = bitrate;
    
    return true;
}

PushStreamConfig PushStreamConfigManager::getPushStreamConfig() {
    PushStreamConfig config;
    if (!loadPushStreamConfig(config)) {
        // 如果加载失败，返回默认值
        config = PushStreamConfig();
    }
    return config;
}

} // namespace detector_service

