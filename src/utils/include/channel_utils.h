#pragma once

#include <string>
#include "channel.h"

namespace detector_service {

/**
 * @brief 将通道状态枚举转换为字符串
 * @param status 通道状态枚举
 * @return 状态字符串
 */
inline std::string channelStatusToString(ChannelStatus status) {
    switch (status) {
        case ChannelStatus::IDLE: return "idle";
        case ChannelStatus::RUNNING: return "running";
        case ChannelStatus::ERROR: return "error";
        case ChannelStatus::STOPPED: return "stopped";
        default: return "unknown";
    }
}

/**
 * @brief 将字符串转换为通道状态枚举
 * @param str 状态字符串
 * @return 通道状态枚举
 */
inline ChannelStatus stringToChannelStatus(const std::string& str) {
    if (str == "idle") return ChannelStatus::IDLE;
    if (str == "running") return ChannelStatus::RUNNING;
    if (str == "error") return ChannelStatus::ERROR;
    if (str == "stopped") return ChannelStatus::STOPPED;
    return ChannelStatus::IDLE;
}

} // namespace detector_service

