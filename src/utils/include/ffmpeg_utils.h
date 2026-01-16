#pragma once

#include <string>
#include <libavutil/error.h>

namespace detector_service {

/**
 * @brief 将FFmpeg错误码转换为字符串
 * @param errnum FFmpeg错误码
 * @return 错误字符串
 */
inline std::string avErrorToString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

} // namespace detector_service

