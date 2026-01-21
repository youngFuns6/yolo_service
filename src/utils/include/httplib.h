#pragma once

// httplib 包装 - 直接使用 vcpkg 安装的 cpp-httplib
// 这个文件提供 httplib::Server 和 httplib::Client 的完整支持

// 注意：由于这个文件在 src/utils/include/ 目录中，当包含 <httplib.h> 时
// 可能会先找到这个文件。我们需要确保包含真正的 httplib 库。

// 尝试包含真正的 httplib（从 vcpkg 安装的）
// 使用 __has_include 来检查是否能够找到真正的 httplib
#if __has_include(<httplib/httplib.h>)
    #include <httplib/httplib.h>
#else
    // 如果上面的路径不存在，尝试直接包含（假设已经在包含路径中）
    // 注意：这需要 CMakeLists.txt 正确设置了 HTTPLIB_INCLUDE_DIR
    #ifndef HTTPLIB_H
        #define HTTPLIB_H
        // 直接包含，假设已经在系统包含路径中
        // CMakeLists.txt 已经通过 include_directories 设置了路径
        #include_next <httplib.h>
    #else
        // 如果已经定义了 HTTPLIB_H，说明真正的 httplib.h 已经被包含
        // 不需要再次包含
    #endif
#endif

// 为了兼容现有代码，提供类型别名
namespace detector_service {
    // 使用 httplib::Server 作为 LwsServer 的别名
    using LwsServer = httplib::Server;
    
    // 为了兼容现有 API 代码，提供 HttpRequest 和 HttpResponse 的别名
    using HttpRequest = httplib::Request;
    using HttpResponse = httplib::Response;
}
