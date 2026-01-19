cmake_minimum_required(VERSION 3.15)

# 在 project() 之前设置编译器（如果使用交叉编译）
# 这会在 vcpkg 工具链文件加载之前设置
# 注意：对于 Linux ARM64 交叉编译，编译器设置主要在 triplet 文件中完成
# 这里只处理环境变量已明确设置的情况
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_CROSSCOMPILING)
    # 检查是否设置了交叉编译器环境变量
    if(DEFINED ENV{CC} AND DEFINED ENV{CXX})
        set(CMAKE_C_COMPILER "$ENV{CC}" CACHE FILEPATH "C compiler")
        set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE FILEPATH "C++ compiler")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 对于 Linux 交叉编译，如果环境变量中设置了 CMAKE_C_COMPILER，则使用它
    # 这确保在 triplet 文件加载之前编译器路径已设置
    if(DEFINED ENV{CMAKE_C_COMPILER} AND DEFINED ENV{CMAKE_CXX_COMPILER})
        # 规范化路径，移除双斜杠等
        get_filename_component(CC_PATH "$ENV{CMAKE_C_COMPILER}" ABSOLUTE)
        get_filename_component(CXX_PATH "$ENV{CMAKE_CXX_COMPILER}" ABSOLUTE)
        set(CMAKE_C_COMPILER "${CC_PATH}" CACHE FILEPATH "C compiler")
        set(CMAKE_CXX_COMPILER "${CXX_PATH}" CACHE FILEPATH "C++ compiler")
        message(STATUS "在 project() 之前设置交叉编译器: ${CMAKE_C_COMPILER}")
    elseif(DEFINED ENV{CC} AND DEFINED ENV{CXX})
        # 如果设置了 CC/CXX 环境变量，也尝试使用
        get_filename_component(CC_PATH "$ENV{CC}" ABSOLUTE)
        get_filename_component(CXX_PATH "$ENV{CXX}" ABSOLUTE)
        set(CMAKE_C_COMPILER "${CC_PATH}" CACHE FILEPATH "C compiler")
        set(CMAKE_CXX_COMPILER "${CXX_PATH}" CACHE FILEPATH "C++ compiler")
        message(STATUS "在 project() 之前从 CC/CXX 设置交叉编译器: ${CMAKE_C_COMPILER}")
    endif()
    
    # 对于交叉编译，预先配置 pthread 库，避免 FindThreads 检测失败
    # 这确保在 project() 调用之前 pthread 配置已设置
    if(CMAKE_CROSSCOMPILING OR (DEFINED ENV{CMAKE_C_COMPILER} AND "$ENV{CMAKE_C_COMPILER}" MATCHES "aarch64"))
        set(CMAKE_USE_PTHREADS_INIT TRUE CACHE INTERNAL "Use pthreads" FORCE)
        set(CMAKE_THREAD_LIBS_INIT "-lpthread" CACHE INTERNAL "Thread libraries" FORCE)
        set(CMAKE_HAVE_LIBC_PTHREAD TRUE CACHE INTERNAL "Have libc pthread" FORCE)
        message(STATUS "在 project() 之前配置 pthread 库: -lpthread")
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

# 检测编译器版本并设置合适的 C++ 标准
# GCC 7+ 和 Clang 5+ 才完全支持 C++17
# 如果编译器版本太旧，降级到 C++14
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0")
        message(WARNING "GCC ${CMAKE_CXX_COMPILER_VERSION} detected. C++17 support is incomplete.")
        message(WARNING "Falling back to C++14. Consider upgrading to GCC 7+ for full C++17 support.")
        set(CMAKE_CXX_STANDARD 14)
        set(CXX_STANDARD_FALLBACK ON)
    else()
        set(CMAKE_CXX_STANDARD 17)
        set(CXX_STANDARD_FALLBACK OFF)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
        message(WARNING "Clang ${CMAKE_CXX_COMPILER_VERSION} detected. C++17 support is incomplete.")
        message(WARNING "Falling back to C++14. Consider upgrading to Clang 5+ for full C++17 support.")
        set(CMAKE_CXX_STANDARD 14)
        set(CXX_STANDARD_FALLBACK ON)
    else()
        set(CMAKE_CXX_STANDARD 17)
        set(CXX_STANDARD_FALLBACK OFF)
    endif()
else()
    # MSVC 和其他编译器，默认使用 C++17
    set(CMAKE_CXX_STANDARD 17)
    set(CXX_STANDARD_FALLBACK OFF)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 确保所有后续目标都使用设置的 C++ 标准（全局设置）
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CXX_EXTENSIONS OFF)

# 显示 C++ 标准设置信息
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(CXX_STANDARD_FALLBACK)
    message(STATUS "Note: Using C++14 due to compiler limitations. Some C++17 features may not be available.")
endif()

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

# 静态链接配置 - 解决 GLIBC 版本不匹配问题
# 选项：是否启用完全静态链接（包括 libc 和 libm）
# 注意：完全静态链接需要将所有共享库也改为静态库
option(STATIC_LINK_ALL "Enable fully static linking (including libc and libm)" OFF)

# 对于 Linux 系统，启用静态链接
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 默认静态链接 libgcc 和 libstdc++，避免 GLIBC 版本问题
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    
    # 如果启用完全静态链接，添加 -static 选项
    # 注意：启用完全静态链接时，所有共享库（utils, models, database, detector）会自动改为静态库
    if(STATIC_LINK_ALL)
        message(STATUS "Enabling fully static linking (including libc and libm)")
        message(STATUS "All shared libraries will be built as static libraries")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        # 注意：完全静态链接可能导致某些系统功能不可用，且文件较大
    else()
        message(STATUS "Using partial static linking (libgcc and libstdc++ only)")
        message(STATUS "To enable fully static linking, use: cmake -DSTATIC_LINK_ALL=ON")
    endif()
endif()

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

# BM1684平台支持选项
option(ENABLE_BM1684 "Enable BM1684 platform support (hardware decode and TPU inference)" OFF)

# BM1684平台特定配置（需要在查找 OpenCV 之前配置）
if(ENABLE_BM1684)
    message(STATUS "BM1684平台支持已启用")
    
    # 查找BM1684 SDK
    # 优先使用 BMNNSDK2_TOP，如果没有则尝试 REL_TOP（官方 docker 常用），最后使用默认路径
    if(DEFINED ENV{BMNNSDK2_TOP})
        set(BMNNSDK2_TOP $ENV{BMNNSDK2_TOP})
    elseif(DEFINED ENV{REL_TOP})
        set(BMNNSDK2_TOP $ENV{REL_TOP})
    else()
        set(BMNNSDK2_TOP $ENV{HOME}/bmnnsdk2/bmnnsdk2-latest)
    endif()
    
    if(EXISTS ${BMNNSDK2_TOP})
        message(STATUS "Found BMNNSDK2 at: ${BMNNSDK2_TOP}")
        
        # 设置BM1684 SDK包含目录
        set(BM1684_INCLUDE_DIRS
            ${BMNNSDK2_TOP}/include
            ${BMNNSDK2_TOP}/include/bmruntime
            ${BMNNSDK2_TOP}/include/bmlib
            ${BMNNSDK2_TOP}/include/third_party/boost/include
        )
        
        # 设置BM1684 SDK OpenCV 和 FFmpeg 包含目录
        set(BM1684_OPENCV_INCLUDE_DIRS ${BMNNSDK2_TOP}/include/opencv/opencv4)
        set(BM1684_FFMPEG_INCLUDE_DIRS 
            ${BMNNSDK2_TOP}/include/ffmpeg 
            ${BMNNSDK2_TOP}/include/bmlib
        )
        
        # 设置BM1684 SDK库目录（根据目标架构）
        if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
            set(BM1684_LIB_DIRS ${BMNNSDK2_TOP}/lib/bmnn/soc)
            set(BM1684_OPENCV_LIB_DIRS ${BMNNSDK2_TOP}/lib/opencv/soc)
            set(BM1684_FFMPEG_LIB_DIRS 
                ${BMNNSDK2_TOP}/lib/ffmpeg/soc
                ${BMNNSDK2_TOP}/lib/decode/soc
                ${BMNNSDK2_TOP}/lib/thirdparty/soc
            )
        else()
            set(BM1684_LIB_DIRS ${BMNNSDK2_TOP}/lib/bmnn/pcie)
            set(BM1684_OPENCV_LIB_DIRS ${BMNNSDK2_TOP}/lib/opencv/pcie)
            set(BM1684_FFMPEG_LIB_DIRS 
                ${BMNNSDK2_TOP}/lib/ffmpeg/pcie
                ${BMNNSDK2_TOP}/lib/decode/pcie
                ${BMNNSDK2_TOP}/lib/thirdparty/x86
            )
        endif()
        
        # 设置BM1684库
        set(BM1684_LIBS bmrt bmcv bmlib)
        
        # 设置BM1684 OpenCV库（SDK提供的OpenCV）
        set(BM1684_OPENCV_LIBS 
            opencv_core 
            opencv_imgproc 
            opencv_imgcodecs 
            opencv_videoio 
            opencv_dnn
        )
        
        # 设置BM1684 FFmpeg库
        set(BM1684_FFMPEG_LIBS 
            avfilter 
            avformat 
            avcodec 
            avutil 
            avdevice 
            swresample 
            swscale
        )
        
        # 添加编译定义
        add_compile_definitions(ENABLE_BM1684)
        if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|amd64")
            add_compile_definitions(BM_PCIE_MODE)
        endif()
        
        message(STATUS "BM1684 include dirs: ${BM1684_INCLUDE_DIRS}")
        message(STATUS "BM1684 lib dirs: ${BM1684_LIB_DIRS}")
        message(STATUS "BM1684 OpenCV include dirs: ${BM1684_OPENCV_INCLUDE_DIRS}")
        message(STATUS "BM1684 OpenCV lib dirs: ${BM1684_OPENCV_LIB_DIRS}")
        message(STATUS "BM1684 FFmpeg include dirs: ${BM1684_FFMPEG_INCLUDE_DIRS}")
        message(STATUS "BM1684 FFmpeg lib dirs: ${BM1684_FFMPEG_LIB_DIRS}")
        
        # 设置 OpenCV 变量供其他模块使用（直接使用库名，让链接器自动查找）
        set(OpenCV_FOUND TRUE)
        set(OpenCV_INCLUDE_DIRS ${BM1684_OPENCV_INCLUDE_DIRS})
        set(OpenCV_LIBS ${BM1684_OPENCV_LIBS})
        
        message(STATUS "使用 BM1684 SDK 提供的 OpenCV")
        message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "OpenCV lib dirs: ${BM1684_OPENCV_LIB_DIRS}")
        message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
    else()
        message(WARNING "BMNNSDK2 not found at ${BMNNSDK2_TOP}, BM1684 support will be disabled")
        set(ENABLE_BM1684 OFF)
    endif()
else()
    message(STATUS "BM1684平台支持已禁用")
endif()

# 查找依赖包
# OpenCV: 如果启用 BM1684，使用 SDK 提供的；否则使用 vcpkg 的
if(NOT ENABLE_BM1684)
    find_package(OpenCV REQUIRED)
    message(STATUS "使用 vcpkg 提供的 OpenCV")
endif()

find_package(Threads REQUIRED)
find_package(unofficial-sqlite3 REQUIRED)
find_package(unofficial-mosquitto CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 查找 cpp-httplib (header-only 库)
# 首先尝试在 vcpkg_installed 目录中查找
if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed")
    file(GLOB VCPKG_TRIPLET_DIRS "${CMAKE_BINARY_DIR}/vcpkg_installed/*")
    foreach(TRIPLET_DIR ${VCPKG_TRIPLET_DIRS})
        if(IS_DIRECTORY "${TRIPLET_DIR}/include")
            find_path(HTTPLIB_INCLUDE_DIR httplib.h
                PATHS "${TRIPLET_DIR}/include"
                NO_DEFAULT_PATH
            )
            if(HTTPLIB_INCLUDE_DIR)
                break()
            endif()
        endif()
    endforeach()
endif()

# 如果还没找到，尝试通过 CMAKE_PREFIX_PATH 查找
if(NOT HTTPLIB_INCLUDE_DIR)
    find_path(HTTPLIB_INCLUDE_DIR httplib.h
        PATHS ${CMAKE_PREFIX_PATH}
        PATH_SUFFIXES include
    )
endif()

# 如果还没找到，尝试系统路径
if(NOT HTTPLIB_INCLUDE_DIR)
    find_path(HTTPLIB_INCLUDE_DIR httplib.h)
endif()

if(HTTPLIB_INCLUDE_DIR)
    message(STATUS "找到 cpp-httplib: ${HTTPLIB_INCLUDE_DIR}")
    include_directories(${HTTPLIB_INCLUDE_DIR})
else()
    message(FATAL_ERROR "未找到 cpp-httplib，请确保 vcpkg 已安装 cpp-httplib")
endif()

# 查找 IXWebSocket
find_package(ixwebsocket CONFIG QUIET)
if(NOT ixwebsocket_FOUND)
    # 尝试在 vcpkg_installed 目录中查找
    if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed")
        file(GLOB VCPKG_TRIPLET_DIRS "${CMAKE_BINARY_DIR}/vcpkg_installed/*")
        foreach(TRIPLET_DIR ${VCPKG_TRIPLET_DIRS})
            if(IS_DIRECTORY "${TRIPLET_DIR}/include")
                find_path(IXWEBSOCKET_INCLUDE_DIR ixwebsocket/IXWebSocketServer.h
                    PATHS "${TRIPLET_DIR}/include"
                    NO_DEFAULT_PATH
                )
                if(IXWEBSOCKET_INCLUDE_DIR)
                    break()
                endif()
            endif()
        endforeach()
    endif()
    
    # 如果还没找到，尝试通过 CMAKE_PREFIX_PATH 查找
    if(NOT IXWEBSOCKET_INCLUDE_DIR)
        find_path(IXWEBSOCKET_INCLUDE_DIR ixwebsocket/IXWebSocketServer.h
            PATHS ${CMAKE_PREFIX_PATH}
            PATH_SUFFIXES include
        )
    endif()
    
    # 如果还没找到，尝试系统路径
    if(NOT IXWEBSOCKET_INCLUDE_DIR)
        find_path(IXWEBSOCKET_INCLUDE_DIR ixwebsocket/IXWebSocketServer.h)
    endif()
    
    if(IXWEBSOCKET_INCLUDE_DIR)
        message(STATUS "找到 IXWebSocket: ${IXWEBSOCKET_INCLUDE_DIR}")
        include_directories(${IXWEBSOCKET_INCLUDE_DIR})
        # IXWebSocket 可能需要链接系统库
        find_library(IXWEBSOCKET_LIB ixwebsocket
            PATHS ${CMAKE_PREFIX_PATH}
            PATH_SUFFIXES lib
        )
        if(IXWEBSOCKET_LIB)
            message(STATUS "找到 IXWebSocket 库: ${IXWEBSOCKET_LIB}")
        endif()
    else()
        message(WARNING "未找到 IXWebSocket，WebSocket 功能可能不可用")
    endif()
else()
    message(STATUS "找到 IXWebSocket (通过 find_package)")
endif()

# onnxruntime 仅在非 BM1684 模式下需要
if(NOT ENABLE_BM1684)
    find_package(onnxruntime CONFIG REQUIRED)
    message(STATUS "onnxruntime 已启用（CPU/GPU 推理模式）")
else()
    message(STATUS "onnxruntime 已禁用（BM1684 TPU 推理模式）")
endif()

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
