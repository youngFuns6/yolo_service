#pragma once

#include <string>
#include <memory>

namespace detector_service {

struct StreamConfig {
    std::string rtmp_url;
    int width = 1920;
    int height = 1080;
    int fps = 25;
    int bitrate = 2000000;
};

class StreamConfigManager {
public:
    static StreamConfigManager& getInstance() {
        static StreamConfigManager instance;
        return instance;
    }

    // 保存推流配置
    bool saveStreamConfig(const StreamConfig& config);
    
    // 加载推流配置
    bool loadStreamConfig(StreamConfig& config);
    
    // 获取推流配置（如果不存在则返回默认值）
    StreamConfig getStreamConfig();

private:
    StreamConfigManager() = default;
    ~StreamConfigManager() = default;
    StreamConfigManager(const StreamConfigManager&) = delete;
    StreamConfigManager& operator=(const StreamConfigManager&) = delete;
};

} // namespace detector_service

