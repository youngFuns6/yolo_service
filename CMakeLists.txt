cmake_minimum_required(VERSION 3.15)

# 在 project() 之前设置编译器（如果使用交叉编译）
# 这会在 vcpkg 工具链文件加载之前设置
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_CROSSCOMPILING)
    # 检查是否设置了交叉编译器环境变量
    if(DEFINED ENV{CC} AND DEFINED ENV{CXX})
        set(CMAKE_C_COMPILER "$ENV{CC}" CACHE FILEPATH "C compiler")
        set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE FILEPATH "C++ compiler")
    endif()
endif()

# 在 project() 之前设置 vcpkg triplet（如果通过 CMake 参数或环境变量传入）
# 这确保在重新配置时 triplet 值被保留
if(DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "vcpkg target triplet")
elseif(DEFINED ENV{VCPKG_TARGET_TRIPLET})
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_TARGET_TRIPLET}" CACHE STRING "vcpkg target triplet")
endif()

project(detector_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 在 project() 之后，清除 vcpkg windows.cmake 设置的 MSVC 风格选项（如果使用 GCC）
# 这对于交叉编译 Windows 目标很重要
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # 清除 MSVC 风格的编译选项，替换为 GCC 风格
    if(CMAKE_C_FLAGS MATCHES "/nologo|/DWIN32|/D_WINDOWS|/utf-8|/MP")
        string(REGEX REPLACE "/nologo" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "/DWIN32" "-DWIN32" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "/D_WINDOWS" "-D_WINDOWS" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "/utf-8" "-finput-charset=UTF-8" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "/MP" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C flags" FORCE)
    endif()
    
    if(CMAKE_CXX_FLAGS MATCHES "/nologo|/DWIN32|/D_WINDOWS|/utf-8|/MP")
        string(REGEX REPLACE "/nologo" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REGEX REPLACE "/DWIN32" "-DWIN32" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REGEX REPLACE "/D_WINDOWS" "-D_WINDOWS" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REGEX REPLACE "/utf-8" "-finput-charset=UTF-8" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REGEX REPLACE "/MP" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "C++ flags" FORCE)
    endif()
    
    # 确保定义了 Windows 宏（使用 GCC 风格）
    if(NOT CMAKE_C_FLAGS MATCHES "-DWIN32")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN32 -D_WINDOWS" CACHE STRING "C flags" FORCE)
    endif()
    if(NOT CMAKE_CXX_FLAGS MATCHES "-DWIN32")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32 -D_WINDOWS" CACHE STRING "C++ flags" FORCE)
    endif()
endif()

# 设置库输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# vcpkg 配置
# 注意：CMAKE_TOOLCHAIN_FILE 应该通过命令行参数在 project() 之前设置
# 这里只用于信息显示和确保 VCPKG_TARGET_TRIPLET 被缓存
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
else()
    message(WARNING "vcpkg not found, using system libraries")
endif()

# 确保 VCPKG_TARGET_TRIPLET 被缓存，以便在重新配置时保留
# 优先使用 CMake 变量，其次使用环境变量
if(DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "vcpkg target triplet" FORCE)
elseif(DEFINED ENV{VCPKG_TARGET_TRIPLET})
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_TARGET_TRIPLET}" CACHE STRING "vcpkg target triplet" FORCE)
endif()

# 添加 cmake 模块路径
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# 如果 vcpkg 工具链文件已加载，设置 vcpkg 安装路径以便查找包
if(CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    # vcpkg 工具链文件已加载，尝试查找 vcpkg_installed 目录
    get_filename_component(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed" ABSOLUTE)
    if(EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
        # 设置 CMAKE_PREFIX_PATH 以便 find_package 能找到 vcpkg 安装的包
        list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
        
        # 设置 OpenCV_DIR
        set(OPENCV_SHARE_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencv4")
        if(EXISTS "${OPENCV_SHARE_DIR}")
            set(OpenCV_DIR "${OPENCV_SHARE_DIR}" CACHE PATH "OpenCV config directory" FORCE)
            message(STATUS "Setting OpenCV_DIR to: ${OpenCV_DIR}")
        endif()
        
        # 添加 FFMPEG 的 FindFFMPEG.cmake 到 CMAKE_MODULE_PATH
        set(FFMPEG_SHARE_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/ffmpeg")
        if(EXISTS "${FFMPEG_SHARE_DIR}")
            list(APPEND CMAKE_MODULE_PATH "${FFMPEG_SHARE_DIR}")
            message(STATUS "Adding FFMPEG module path: ${FFMPEG_SHARE_DIR}")
        endif()
        
        message(STATUS "Using vcpkg from: ${VCPKG_ROOT}")
        message(STATUS "Using vcpkg triplet: ${VCPKG_TARGET_TRIPLET}")
        message(STATUS "vcpkg installed dir: ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    endif()
endif()

# 查找依赖包
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
find_package(unofficial-sqlite3 REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(onnxruntime CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(unofficial-mosquitto CONFIG REQUIRED)

# 查找或编译 eXosip2（GB28181 SIP支持）
find_package(Exosip2)
if(EXOSIP2_FOUND)
    message(STATUS "eXosip2 found or will be built from source")
else()
    message(WARNING "eXosip2 not found, GB28181 SIP features will be disabled")
endif()

# 包含基础目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

# ============================================================================
# 添加子模块 - 按依赖顺序添加
# ============================================================================

# 0. 配置库 (仅头文件，无依赖)
add_subdirectory(src/config)

# 1. 工具库 (无依赖)
add_subdirectory(src/utils)

# 2. 模型库 (依赖 Threads)
add_subdirectory(src/models)

# 3. 数据库库 (依赖 SQLite3)
add_subdirectory(src/database)

# 4. 检测器库 (依赖 OpenCV, ONNX Runtime, utils)
add_subdirectory(src/detector)

# 5. 推流库 (依赖 models, detector, utils) - 包含流管理和帧回调
add_subdirectory(src/stream)

# 6. API 库 (依赖 models, database, detector, stream) - 包含 HTTP API 和 WebSocket
add_subdirectory(src/api)

# 7. 服务库 (依赖 database, detector, stream, api) - 服务初始化和启动
add_subdirectory(src/service)

# 10. 主程序 (依赖所有库)
add_subdirectory(src)
