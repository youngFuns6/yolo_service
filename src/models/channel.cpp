#include "channel.h"
#include "database.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace detector_service {

std::string getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string channelStatusToString(ChannelStatus status) {
    switch (status) {
        case ChannelStatus::IDLE: return "idle";
        case ChannelStatus::RUNNING: return "running";
        case ChannelStatus::ERROR: return "error";
        case ChannelStatus::STOPPED: return "stopped";
        default: return "unknown";
    }
}

ChannelStatus stringToChannelStatus(const std::string& str) {
    if (str == "idle") return ChannelStatus::IDLE;
    if (str == "running") return ChannelStatus::RUNNING;
    if (str == "error") return ChannelStatus::ERROR;
    if (str == "stopped") return ChannelStatus::STOPPED;
    return ChannelStatus::IDLE;
}

int ChannelManager::createChannel(const Channel& channel) {
    auto& db = Database::getInstance();
    
    int id;
    if (channel.id > 0) {
        // 如果用户指定了id，使用用户指定的id
        id = channel.id;
        // 检查数据库中id是否已存在
        std::string name, source_url, status, created_at, updated_at;
        bool enabled, push_enabled, report_enabled;
        if (db.loadChannelFromDB(id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
            return -1; // 返回-1表示id已存在
        }
    } else {
        // 如果用户没有指定id，自动生成（从数据库获取最大id+1）
        int max_id = db.getMaxChannelId();
        id = max_id + 1;
    }
    
    std::string created_at = getCurrentTime();
    std::string updated_at = created_at;
    
    // 保存到数据库
    int db_result = db.insertChannel(id, channel.name, channel.source_url,
                                      channel.enabled.load(), channel.push_enabled.load(),
                                      channel.report_enabled.load(), created_at, updated_at);
    if (db_result == -1) {
        std::cerr << "错误: 通道数据库保存失败" << std::endl;
        return -1;
    }
    
    return id;
}

bool ChannelManager::deleteChannel(int channel_id) {
    auto& db = Database::getInstance();
    
    // 检查通道是否存在
    std::string name, source_url, status, created_at, updated_at;
    bool enabled, push_enabled, report_enabled;
    if (!db.loadChannelFromDB(channel_id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
        return false; // 通道不存在
    }
    
    // 如果通道正在运行，先更新数据库状态为停止
    if (status == "running") {
        std::string updated_at = getCurrentTime();
        if (!db.updateChannelStatus(channel_id, channelStatusToString(ChannelStatus::STOPPED), updated_at)) {
            std::cerr << "错误: 通道状态更新失败，不删除通道" << std::endl;
            return false;
        }
    }
    
    // 从数据库删除
    bool db_result = db.deleteChannel(channel_id);
    if (!db_result) {
        std::cerr << "错误: 通道数据库删除失败" << std::endl;
        return false;
    }
    
    return true;
}

bool ChannelManager::updateChannel(int channel_id, const Channel& channel) {
    auto& db = Database::getInstance();
    
    // 检查通道是否存在
    std::string name, source_url, status, created_at, updated_at;
    bool enabled, push_enabled, report_enabled;
    if (!db.loadChannelFromDB(channel_id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
        return false; // 通道不存在
    }
    
    int final_id = channel_id;
    
    // 如果id被修改了，需要先更新数据库中的id
    if (channel.id != channel_id && channel.id > 0) {
        // 检查新id是否已存在
        std::string tmp_name, tmp_source_url, tmp_status, tmp_created_at, tmp_updated_at;
        bool tmp_enabled, tmp_push_enabled, tmp_report_enabled;
        if (db.loadChannelFromDB(channel.id, tmp_name, tmp_source_url, tmp_status, tmp_enabled, tmp_push_enabled, tmp_report_enabled, tmp_created_at, tmp_updated_at)) {
            return false; // 新id已存在，更新失败
        }
        
        // 更新数据库中的id
        bool db_result = db.updateChannelId(channel_id, channel.id);
        if (!db_result) {
            std::cerr << "错误: 通道ID数据库更新失败" << std::endl;
            return false;
        }
        
        final_id = channel.id;
    }
    
    updated_at = getCurrentTime();
    
    // 更新数据库
    bool db_result = db.updateChannel(final_id, channel.name, channel.source_url,
                                      channel.enabled.load(), channel.push_enabled.load(),
                                      channel.report_enabled.load(), updated_at);
    if (!db_result) {
        std::cerr << "错误: 通道数据库更新失败" << std::endl;
        // 如果之前更新了id，需要回滚
        if (final_id != channel_id) {
            db.updateChannelId(final_id, channel_id);
        }
        return false;
    }
    
    return true;
}

std::shared_ptr<Channel> ChannelManager::getChannel(int channel_id) {
    auto& db = Database::getInstance();
    
    std::string name, source_url, status_str, created_at, updated_at;
    bool enabled, push_enabled, report_enabled;
    
    if (!db.loadChannelFromDB(channel_id, name, source_url, status_str, enabled, push_enabled, report_enabled, created_at, updated_at)) {
        return nullptr; // 通道不存在
    }
    
    auto channel = std::make_shared<Channel>();
    channel->id = channel_id;
    channel->name = name;
    channel->source_url = source_url;
    channel->status = stringToChannelStatus(status_str);
    channel->enabled = enabled;
    channel->push_enabled = push_enabled;
    channel->report_enabled = report_enabled;
    channel->created_at = created_at;
    channel->updated_at = updated_at;
    
    return channel;
}

std::vector<std::shared_ptr<Channel>> ChannelManager::getAllChannels() {
    auto& db = Database::getInstance();
    auto channel_list = db.getAllChannelsFromDB();
    
    std::vector<std::shared_ptr<Channel>> result;
    result.reserve(channel_list.size());
    
    for (const auto& pair : channel_list) {
        int channel_id = pair.first;
        
        std::string name, source_url, status_str, created_at, updated_at;
        bool enabled, push_enabled, report_enabled;
        
        if (db.loadChannelFromDB(channel_id, name, source_url, status_str, enabled, push_enabled, report_enabled, created_at, updated_at)) {
            auto channel = std::make_shared<Channel>();
            channel->id = channel_id;
            channel->name = name;
            channel->source_url = source_url;
            channel->status = stringToChannelStatus(status_str);
            channel->enabled = enabled;
            channel->push_enabled = push_enabled;
            channel->report_enabled = report_enabled;
            channel->created_at = created_at;
            channel->updated_at = updated_at;
            
            result.push_back(channel);
        }
    }
    
    return result;
}

bool ChannelManager::startChannel(int channel_id) {
    auto& db = Database::getInstance();
    
    // 检查通道是否存在并获取当前状态
    std::string name, source_url, status, created_at, updated_at;
    bool enabled, push_enabled, report_enabled;
    if (!db.loadChannelFromDB(channel_id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
        return false; // 通道不存在
    }
    
    // 如果已经是运行状态，直接返回成功
    if (status == "running") {
        return true;
    }
    
    // 更新数据库状态
    std::string new_updated_at = getCurrentTime();
    bool db_result = db.updateChannelStatus(channel_id, channelStatusToString(ChannelStatus::RUNNING),
                                            new_updated_at);
    if (!db_result) {
        std::cerr << "错误: 通道状态数据库更新失败" << std::endl;
        return false;
    }
    
    return true;
}

bool ChannelManager::stopChannel(int channel_id) {
    auto& db = Database::getInstance();
    
    // 检查通道是否存在并获取当前状态
    std::string name, source_url, status, created_at, updated_at;
    bool enabled, push_enabled, report_enabled;
    if (!db.loadChannelFromDB(channel_id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
        return false; // 通道不存在
    }
    
    // 如果已经是停止状态，直接返回成功
    if (status != "running") {
        return true;
    }
    
    // 更新数据库状态
    std::string new_updated_at = getCurrentTime();
    bool db_result = db.updateChannelStatus(channel_id, channelStatusToString(ChannelStatus::STOPPED),
                                            new_updated_at);
    if (!db_result) {
        std::cerr << "错误: 通道状态数据库更新失败" << std::endl;
        return false;
    }
    
    return true;
}

bool ChannelManager::isChannelRunning(int channel_id) {
    auto& db = Database::getInstance();
    
    std::string name, source_url, status, created_at, updated_at;
    bool enabled, push_enabled, report_enabled;
    
    if (!db.loadChannelFromDB(channel_id, name, source_url, status, enabled, push_enabled, report_enabled, created_at, updated_at)) {
        return false; // 通道不存在
    }
    
    return status == "running";
}


} // namespace detector_service

