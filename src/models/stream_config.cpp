#include "stream_config.h"
#include "database.h"
#include <iostream>

namespace detector_service {

bool StreamConfigManager::saveStreamConfig(const StreamConfig& config) {
    auto& db = Database::getInstance();
    return db.saveStreamConfig(config.rtmp_url, config.width, config.height, config.fps, config.bitrate);
}

bool StreamConfigManager::loadStreamConfig(StreamConfig& config) {
    auto& db = Database::getInstance();
    
    std::string rtmp_url;
    int width, height, fps, bitrate;
    
    if (!db.loadStreamConfig(rtmp_url, width, height, fps, bitrate)) {
        return false;
    }
    
    config.rtmp_url = rtmp_url;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.bitrate = bitrate;
    
    return true;
}

StreamConfig StreamConfigManager::getStreamConfig() {
    StreamConfig config;
    if (!loadStreamConfig(config)) {
        // 如果加载失败，返回默认值
        config = StreamConfig();
    }
    return config;
}

} // namespace detector_service

