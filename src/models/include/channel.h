#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <vector>

namespace detector_service {

enum class ChannelStatus {
    IDLE,
    RUNNING,
    ERROR,
    STOPPED
};

struct Channel {
    int id;
    std::string name;
    std::string source_url;
    ChannelStatus status;
    std::atomic<bool> enabled;
    std::atomic<bool> push_enabled;  // 推流开关
    int width;
    int height;
    int fps;
    std::string created_at;
    std::string updated_at;

    Channel() : id(0), status(ChannelStatus::IDLE), enabled(false), push_enabled(false),
                width(1920), height(1080), fps(25) {}
    
    // 自定义拷贝构造函数，处理 std::atomic 成员
    Channel(const Channel& other) 
        : id(other.id),
          name(other.name),
          source_url(other.source_url),
          status(other.status),
          enabled(other.enabled.load()),
          push_enabled(other.push_enabled.load()),
          width(other.width),
          height(other.height),
          fps(other.fps),
          created_at(other.created_at),
          updated_at(other.updated_at) {}
};

class ChannelManager {
public:
    static ChannelManager& getInstance() {
        static ChannelManager instance;
        return instance;
    }

    // 通道增删改查
    int createChannel(const Channel& channel);
    bool deleteChannel(int channel_id);
    bool updateChannel(int channel_id, const Channel& channel);
    std::shared_ptr<Channel> getChannel(int channel_id);
    std::vector<std::shared_ptr<Channel>> getAllChannels();
    
    // 通道状态管理
    bool startChannel(int channel_id);
    bool stopChannel(int channel_id);
    bool isChannelRunning(int channel_id);

private:
    ChannelManager() = default;
    ~ChannelManager() = default;
    ChannelManager(const ChannelManager&) = delete;
    ChannelManager& operator=(const ChannelManager&) = delete;
};

} // namespace detector_service

