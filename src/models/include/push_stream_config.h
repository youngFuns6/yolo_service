#pragma once

#include <string>
#include <memory>
#include <optional>

namespace detector_service {

struct PushStreamConfig {
    std::string rtmp_url;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> fps;
    std::optional<int> bitrate;
};

class PushStreamConfigManager {
public:
    static PushStreamConfigManager& getInstance() {
        static PushStreamConfigManager instance;
        return instance;
    }

    // 保存推流配置
    bool savePushStreamConfig(const PushStreamConfig& config);
    
    // 加载推流配置
    bool loadPushStreamConfig(PushStreamConfig& config);
    
    // 获取推流配置（如果不存在则返回默认值）
    PushStreamConfig getPushStreamConfig();

private:
    PushStreamConfigManager() = default;
    ~PushStreamConfigManager() = default;
    PushStreamConfigManager(const PushStreamConfigManager&) = delete;
    PushStreamConfigManager& operator=(const PushStreamConfigManager&) = delete;
};

} // namespace detector_service

