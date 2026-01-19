#pragma once

#include <string>

namespace detector_service {

/**
 * @brief GB28181通道编码工具类
 * 提供通道编码的生成和解析功能
 */
class GB28181ChannelCode {
public:
    /**
     * @brief 生成通道编码
     * @param device_id 设备ID（20位国标编码）
     * @param channel_id 通道ID（数字）
     * @param type_code 通道类型码（如"131"表示前端设备通道，"132"表示视频通道等）
     * @return 20位通道编码，失败返回空字符串
     * 
     * 编码格式：设备ID前10位 + 类型码(3位) + 通道ID(4位,补零) + 设备ID后3位
     */
    static std::string generateChannelCode(const std::string& device_id, 
                                          int channel_id, 
                                          const std::string& type_code = "132");
    
    /**
     * @brief 从通道编码中提取通道ID
     * @param channel_code 20位通道编码
     * @return 通道ID，失败返回0
     * 
     * 通道ID位于编码的第13-16位（索引13-16）
     */
    static int extractChannelId(const std::string& channel_code);
    
    /**
     * @brief 验证通道编码格式
     * @param channel_code 通道编码
     * @return 是否为有效的20位编码
     */
    static bool isValidChannelCode(const std::string& channel_code);
};

} // namespace detector_service
