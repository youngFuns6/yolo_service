#include "gb28181_utils.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace detector_service {

std::string GB28181ChannelCode::generateChannelCode(const std::string& device_id, 
                                                    int channel_id, 
                                                    const std::string& type_code) {
    // 验证设备ID长度（应该是20位）
    if (device_id.length() < 10) {
        return "";
    }
    
    // 验证类型码长度（应该是3位）
    if (type_code.length() != 3) {
        return "";
    }
    
    // 验证通道ID范围（应该是0-9999）
    if (channel_id < 0 || channel_id > 9999) {
        return "";
    }
    
    std::ostringstream oss;
    
    // 设备ID前10位
    oss << device_id.substr(0, 10);
    
    // 类型码（3位）
    oss << type_code;
    
    // 通道ID（4位，补零）
    oss << std::setw(4) << std::setfill('0') << channel_id;
    
    // 设备ID后3位
    if (device_id.length() >= 20) {
        // 标准20位设备ID，取第17-19位（索引17-19）
        oss << device_id.substr(17, 3);
    } else if (device_id.length() >= 13) {
        // 设备ID长度>=13位，取最后3位
        oss << device_id.substr(device_id.length() - 3, 3);
    } else {
        // 如果设备ID长度不足13位，用0补齐到3位
        int remaining = 3 - (device_id.length() - 10);
        if (remaining > 0) {
            oss << std::string(remaining, '0');
        }
    }
    
    return oss.str();
}

int GB28181ChannelCode::extractChannelId(const std::string& channel_code) {
    // 验证通道编码长度（应该是20位）
    if (channel_code.length() < 17) {
        return 0;
    }
    
    try {
        // 通道ID位于第13-16位（索引13-16）
        std::string channel_str = channel_code.substr(13, 4);
        return std::stoi(channel_str);
    } catch (const std::exception&) {
        return 0;
    }
}

bool GB28181ChannelCode::isValidChannelCode(const std::string& channel_code) {
    // 验证长度（应该是20位）
    if (channel_code.length() != 20) {
        return false;
    }
    
    // 验证是否全为数字
    for (char c : channel_code) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    
    return true;
}

} // namespace detector_service
