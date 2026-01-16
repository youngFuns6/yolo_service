#pragma once

#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace detector_service {

/**
 * @brief 获取当前时间字符串
 * @return 格式化的时间字符串 (YYYY-MM-DD HH:MM:SS)
 */
inline std::string getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief 解码URL中的HTML实体编码
 * @param url 原始URL字符串
 * @return 解码后的URL字符串
 */
inline std::string decodeUrlEntities(const std::string& url) {
    std::string result = url;
    
    // 替换常见的HTML实体编码
    // &amp; -> &
    size_t pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
        result.replace(pos, 5, "&");
        pos += 1;
    }
    
    // 可以添加更多HTML实体解码，如：
    // &lt; -> <
    // &gt; -> >
    // &quot; -> "
    // &#39; -> '
    
    return result;
}

} // namespace detector_service

