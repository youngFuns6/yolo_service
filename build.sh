#!/bin/bash

# 视觉分析服务构建脚本
# 支持多平台交叉编译

set -e  # 遇到错误立即退出

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默认配置
PLATFORM="linux"
ARCH="x64"
BUILD_TYPE="Release"
VCPKG_TRIPLET=""
CUSTOM_TRIPLET=""
CLEAN=false
JOBS=$(nproc 2>/dev/null || echo 4)
NO_PROXY=false
TARGET_SYSROOT=""

# 显示帮助信息
show_help() {
    cat << EOF
视觉分析服务构建脚本

用法: $0 [选项]

选项:
    -p, --platform PLATFORM    目标平台 (linux|windows|macos|android) [默认: linux]
    -a, --arch ARCH              目标架构 (x64|x86|arm64|armv7) [默认: x64]
    -t, --type TYPE              构建类型 (Debug|Release|RelWithDebInfo) [默认: Release]
    -c, --clean                  清理构建目录
    -j, --jobs N                 并行编译任务数 [默认: CPU核心数]
    --sysroot PATH               目标系统 sysroot 路径（用于交叉编译兼容性）
    --no-proxy                   禁用代理（用于 vcpkg 下载）
    -h, --help                   显示此帮助信息

平台和架构组合示例:
    Linux x64:     -p linux -a x64
    Linux ARM64:   -p linux -a arm64
    Windows x64:   -p windows -a x64
    macOS ARM64:   -p macos -a arm64
    Android ARM64: -p android -a arm64

示例:
    $0                          # Linux x64 Release
    $0 -p windows -a x64        # Windows x64 Release
    $0 -p linux -a arm64 -t Debug  # Linux ARM64 Debug
    $0 -p linux -a arm64 --sysroot /path/to/sysroot  # 使用目标系统 sysroot
    $0 -c                       # 清理构建目录

GLIBC 兼容性:
    如果目标系统的 GLIBC 版本较旧，请使用 --sysroot 指定目标系统的 sysroot。
    详见 GLIBC_COMPATIBILITY.md 文档。
EOF
}

# 解析命令行参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -p|--platform)
                PLATFORM="$2"
                shift 2
                ;;
            -a|--arch)
                ARCH="$2"
                shift 2
                ;;
            -t|--type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            -c|--clean)
                CLEAN=true
                shift
                ;;
            -j|--jobs)
                JOBS="$2"
                shift 2
                ;;
            --sysroot)
                TARGET_SYSROOT="$2"
                shift 2
                ;;
            --no-proxy)
                NO_PROXY=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
        esac
    done
}

# 确定 vcpkg triplet
determine_triplet() {
    case "${PLATFORM}-${ARCH}" in
        linux-x64)
            VCPKG_TRIPLET="x64-linux"
            ;;
        linux-x86)
            VCPKG_TRIPLET="x86-linux"
            ;;
        linux-arm64)
            # 在非 ARM64 主机上交叉编译时，使用自定义 triplet
            if [ "$(uname -m)" != "aarch64" ]; then
                # 检查是否有自定义 triplet 文件
                if [ -f "$SCRIPT_DIR/triplets/arm64-linux.cmake" ]; then
                    VCPKG_TRIPLET="arm64-linux"
                    CUSTOM_TRIPLET="$SCRIPT_DIR/triplets/arm64-linux.cmake"
                else
                    echo -e "${YELLOW}警告: 未找到自定义 triplet，尝试使用标准 triplet${NC}"
                    VCPKG_TRIPLET="arm64-linux"
                fi
            else
                VCPKG_TRIPLET="arm64-linux"
            fi
            ;;
        linux-armv7)
            VCPKG_TRIPLET="arm-linux"
            ;;
        windows-x64)
            # 在非 Windows 主机上交叉编译时，使用自定义 triplet
            if [ "$(uname -s)" != "MINGW"* ] && [ "$(uname -s)" != "MSYS"* ] && [ "$(uname -s)" != "CYGWIN"* ]; then
                # 检查是否有自定义 triplet 文件
                if [ -f "$SCRIPT_DIR/triplets/x64-windows-cross.cmake" ]; then
                    VCPKG_TRIPLET="x64-windows-cross"
                    CUSTOM_TRIPLET="$SCRIPT_DIR/triplets/x64-windows-cross.cmake"
                else
                    echo -e "${YELLOW}警告: 未找到自定义 triplet，尝试使用标准 triplet${NC}"
                    VCPKG_TRIPLET="x64-windows"
                fi
            else
                VCPKG_TRIPLET="x64-windows"
            fi
            ;;
        windows-x86)
            # 在非 Windows 主机上交叉编译时，使用自定义 triplet
            if [ "$(uname -s)" != "MINGW"* ] && [ "$(uname -s)" != "MSYS"* ] && [ "$(uname -s)" != "CYGWIN"* ]; then
                echo -e "${YELLOW}警告: Windows x86 交叉编译需要自定义 triplet，当前使用标准 triplet${NC}"
            fi
            VCPKG_TRIPLET="x86-windows"
            ;;
        windows-arm64)
            VCPKG_TRIPLET="arm64-windows"
            ;;
        macos-x64)
            VCPKG_TRIPLET="x64-osx"
            ;;
        macos-arm64)
            VCPKG_TRIPLET="arm64-osx"
            ;;
        android-arm64)
            VCPKG_TRIPLET="arm64-android"
            ;;
        android-armv7)
            VCPKG_TRIPLET="arm-android"
            ;;
        *)
            echo -e "${RED}不支持的平台-架构组合: ${PLATFORM}-${ARCH}${NC}"
            exit 1
            ;;
    esac
}

# 检查 CMake
check_cmake() {
    if ! command -v cmake >/dev/null 2>&1; then
        echo -e "${RED}错误: 未找到 CMake${NC}"
        echo -e "${YELLOW}请安装 CMake:${NC}"
        echo -e "${BLUE}  macOS:   brew install cmake${NC}"
        echo -e "${BLUE}  Linux:   sudo apt-get install cmake${NC}"
        echo -e "${BLUE}  Windows: 从 https://cmake.org/download/ 下载安装${NC}"
        exit 1
    fi
    
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo -e "${GREEN}找到 CMake: $CMAKE_VERSION${NC}"
}

# 检查并处理代理设置
check_proxy() {
    if [ "$NO_PROXY" = true ]; then
        echo -e "${YELLOW}禁用代理设置（用于 vcpkg 下载）${NC}"
        unset HTTP_PROXY
        unset HTTPS_PROXY
        unset http_proxy
        unset https_proxy
        unset ALL_PROXY
        unset all_proxy
        export HTTP_PROXY=""
        export HTTPS_PROXY=""
        export http_proxy=""
        export https_proxy=""
        export ALL_PROXY=""
        export all_proxy=""
    else
        # 统一处理大小写的代理环境变量
        # 优先使用已设置的环境变量，如果没有则使用默认值
        HTTP_PROXY_VAL="${HTTP_PROXY:-${http_proxy}}"
        HTTPS_PROXY_VAL="${HTTPS_PROXY:-${https_proxy}}"
        ALL_PROXY_VAL="${ALL_PROXY:-${all_proxy}}"
        
        # 如果用户已经设置了代理，使用用户的设置
        if [ -n "$HTTP_PROXY_VAL" ] || [ -n "$HTTPS_PROXY_VAL" ] || [ -n "$ALL_PROXY_VAL" ]; then
            # 统一设置所有代理环境变量（大小写都设置，确保兼容性）
            if [ -n "$HTTP_PROXY_VAL" ]; then
                export HTTP_PROXY="$HTTP_PROXY_VAL"
                export http_proxy="$HTTP_PROXY_VAL"
            fi
            if [ -n "$HTTPS_PROXY_VAL" ]; then
                export HTTPS_PROXY="$HTTPS_PROXY_VAL"
                export https_proxy="$HTTPS_PROXY_VAL"
            fi
            if [ -n "$ALL_PROXY_VAL" ]; then
                export ALL_PROXY="$ALL_PROXY_VAL"
                export all_proxy="$ALL_PROXY_VAL"
            fi
            
            PROXY_URL="${HTTPS_PROXY_VAL:-${HTTP_PROXY_VAL:-${ALL_PROXY_VAL}}}"
            echo -e "${BLUE}检测到代理设置: ${PROXY_URL}${NC}"
            
            # 提取代理地址和端口用于连接测试
            PROXY_HOST=""
            PROXY_PORT=""
            if [[ "$PROXY_URL" =~ ^https?://([^:/]+):([0-9]+) ]]; then
                PROXY_HOST="${BASH_REMATCH[1]}"
                PROXY_PORT="${BASH_REMATCH[2]}"
            elif [[ "$PROXY_URL" =~ ^socks5://([^:/]+):([0-9]+) ]]; then
                PROXY_HOST="${BASH_REMATCH[1]}"
                PROXY_PORT="${BASH_REMATCH[2]}"
            fi
            
            # 检查代理是否可达（使用 nc 或 timeout，如果都不可用则跳过检查）
            if [ -n "$PROXY_HOST" ] && [ -n "$PROXY_PORT" ]; then
                if command -v nc >/dev/null 2>&1; then
                    if ! timeout 2 nc -z "$PROXY_HOST" "$PROXY_PORT" 2>/dev/null; then
                        echo -e "${YELLOW}警告: 代理 ${PROXY_URL} 不可达${NC}"
                        echo -e "${YELLOW}建议: 使用 --no-proxy 选项禁用代理，或确保代理服务正在运行${NC}"
                        echo -e "${BLUE}继续尝试构建，但可能会失败...${NC}"
                    else
                        echo -e "${GREEN}代理连接正常${NC}"
                    fi
                elif command -v timeout >/dev/null 2>&1; then
                    # 使用 timeout + bash 内置 /dev/tcp（如果支持）
                    if ! timeout 2 bash -c "echo > /dev/tcp/$PROXY_HOST/$PROXY_PORT" 2>/dev/null; then
                        echo -e "${YELLOW}警告: 代理 ${PROXY_URL} 可能不可达${NC}"
                        echo -e "${YELLOW}建议: 使用 --no-proxy 选项禁用代理，或确保代理服务正在运行${NC}"
                        echo -e "${BLUE}继续尝试构建...${NC}"
                    else
                        echo -e "${GREEN}代理连接正常${NC}"
                    fi
                else
                    echo -e "${BLUE}无法检测代理连接性，继续构建...${NC}"
                fi
            fi
        else
            echo -e "${BLUE}未检测到代理设置${NC}"
        fi
    fi
}

# 检查 vcpkg
check_vcpkg() {
    if [ -z "$VCPKG_ROOT" ]; then
        echo -e "${YELLOW}警告: VCPKG_ROOT 环境变量未设置${NC}"
        echo -e "${YELLOW}尝试查找 vcpkg...${NC}"
        
        # 尝试常见位置
        if [ -d "$HOME/vcpkg" ]; then
            export VCPKG_ROOT="$HOME/vcpkg"
            echo -e "${GREEN}找到 vcpkg: $VCPKG_ROOT${NC}"
        elif [ -d "/usr/local/vcpkg" ]; then
            export VCPKG_ROOT="/usr/local/vcpkg"
            echo -e "${GREEN}找到 vcpkg: $VCPKG_ROOT${NC}"
        else
            echo -e "${RED}错误: 未找到 vcpkg，请设置 VCPKG_ROOT 环境变量${NC}"
            exit 1
        fi
    fi
    
    if [ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
        echo -e "${RED}错误: vcpkg 未正确安装: $VCPKG_ROOT${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}使用 vcpkg: $VCPKG_ROOT${NC}"
}

# 清理构建目录
clean_build() {
    echo -e "${BLUE}清理构建目录...${NC}"
    if [ -d "build" ]; then
        rm -rf build
        echo -e "${GREEN}构建目录已清理${NC}"
    else
        echo -e "${YELLOW}构建目录不存在${NC}"
    fi
}

# 创建构建目录
create_build_dir() {
    BUILD_DIR="build/${PLATFORM}-${ARCH}-${BUILD_TYPE}"
    mkdir -p "$BUILD_DIR"
    echo -e "${GREEN}构建目录: $BUILD_DIR${NC}"
}

# 检测并设置 CMake 生成器
detect_cmake_generator() {
    # 优先使用 Ninja（更快）
    if command -v ninja >/dev/null 2>&1; then
        echo -e "${GREEN}使用 Ninja 生成器${NC}" >&2
        echo "Ninja"
    # 其次使用 Unix Makefiles
    elif command -v make >/dev/null 2>&1; then
        echo -e "${GREEN}使用 Unix Makefiles 生成器${NC}" >&2
        echo "Unix Makefiles"
    else
        echo -e "${RED}错误: 未找到构建工具（ninja 或 make）${NC}" >&2
        echo -e "${YELLOW}请安装其中一个:${NC}" >&2
        echo -e "${BLUE}  macOS:   brew install ninja${NC}" >&2
        echo -e "${BLUE}  Linux:   sudo apt-get install ninja-build${NC}" >&2
        exit 1
    fi
}

# 配置 CMake
configure_cmake() {
    echo -e "${BLUE}配置 CMake...${NC}"
    echo -e "${BLUE}平台: ${PLATFORM}${NC}"
    echo -e "${BLUE}架构: ${ARCH}${NC}"
    echo -e "${BLUE}构建类型: ${BUILD_TYPE}${NC}"
    echo -e "${BLUE}vcpkg triplet: ${VCPKG_TRIPLET}${NC}"
    
    # 检测 CMake 生成器
    CMAKE_GENERATOR=$(detect_cmake_generator)
    
    # 检查构建目录中是否已有 CMakeCache.txt，如果生成器不匹配则清理
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        CACHED_GENERATOR=$(grep "^CMAKE_GENERATOR:INTERNAL=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d'=' -f2)
        if [ -n "$CACHED_GENERATOR" ] && [ "$CACHED_GENERATOR" != "$CMAKE_GENERATOR" ]; then
            echo -e "${YELLOW}警告: 检测到生成器不匹配（缓存: $CACHED_GENERATOR，当前: $CMAKE_GENERATOR）${NC}"
            echo -e "${BLUE}清理 CMake 缓存...${NC}"
            rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
        fi
        # 检查 ARM64 交叉编译时，如果 CMAKE_AR 被缓存到构建目录中（错误），则清理缓存
        if [ "$ARCH" = "arm64" ] && [ "$(uname -m)" != "aarch64" ]; then
            CACHED_AR=$(grep "^CMAKE_AR:FILEPATH=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d'=' -f2)
            if [ -n "$CACHED_AR" ] && echo "$CACHED_AR" | grep -q "$BUILD_DIR"; then
                echo -e "${YELLOW}警告: 检测到 CMAKE_AR 缓存路径错误，清理 CMake 缓存...${NC}"
                rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
            fi
        fi
    fi
    
    cd "$BUILD_DIR"
    
    CMAKE_ARGS=(
        -G "$CMAKE_GENERATOR"
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
        -DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )
    
    # 如果使用自定义 triplet，需要将其复制到 vcpkg 的 triplets 目录或通过环境变量指定
    if [ -n "$CUSTOM_TRIPLET" ] && [ -f "$CUSTOM_TRIPLET" ]; then
        # 将自定义 triplet 复制到 vcpkg triplets 目录
        VCPKG_TRIPLETS_DIR="$VCPKG_ROOT/triplets"
        VCPKG_TOOLCHAINS_DIR="$VCPKG_ROOT/scripts/toolchains"
        TRIPLET_NAME=$(basename "$CUSTOM_TRIPLET")
        
        if [ ! -f "$VCPKG_TRIPLETS_DIR/$TRIPLET_NAME" ]; then
            echo -e "${BLUE}安装自定义 triplet: $TRIPLET_NAME${NC}"
            cp "$CUSTOM_TRIPLET" "$VCPKG_TRIPLETS_DIR/$TRIPLET_NAME"
        fi
        
        # 检查并复制工具链文件（如果存在）
        # 优先复制包装工具链文件
        WRAPPER_TOOLCHAIN_FILE="$SCRIPT_DIR/toolchains/mingw-w64-wrapper.cmake"
        TOOLCHAIN_FILE="$SCRIPT_DIR/toolchains/mingw-w64-cross.cmake"
        
        if [ -f "$WRAPPER_TOOLCHAIN_FILE" ]; then
            TOOLCHAIN_NAME=$(basename "$WRAPPER_TOOLCHAIN_FILE")
            if [ ! -f "$VCPKG_TOOLCHAINS_DIR/$TOOLCHAIN_NAME" ]; then
                echo -e "${BLUE}安装包装工具链文件: $TOOLCHAIN_NAME${NC}"
                mkdir -p "$VCPKG_TOOLCHAINS_DIR"
                cp "$WRAPPER_TOOLCHAIN_FILE" "$VCPKG_TOOLCHAINS_DIR/$TOOLCHAIN_NAME"
            fi
        fi
        
        if [ -f "$TOOLCHAIN_FILE" ]; then
            TOOLCHAIN_NAME=$(basename "$TOOLCHAIN_FILE")
            if [ ! -f "$VCPKG_TOOLCHAINS_DIR/$TOOLCHAIN_NAME" ]; then
                echo -e "${BLUE}安装工具链文件: $TOOLCHAIN_NAME${NC}"
                mkdir -p "$VCPKG_TOOLCHAINS_DIR"
                cp "$TOOLCHAIN_FILE" "$VCPKG_TOOLCHAINS_DIR/$TOOLCHAIN_NAME"
            fi
        fi
    fi
    
    # macOS 本地编译配置
    if [ "$PLATFORM" = "macos" ] && [ "$(uname -s)" = "Darwin" ]; then
        # 确保使用正确的编译器
        if [ -z "$CC" ]; then
            # 尝试使用 clang（macOS 默认）
            if command -v clang >/dev/null 2>&1; then
                export CC=clang
                export CXX=clang++
                echo -e "${GREEN}使用编译器: clang/clang++${NC}"
            fi
        fi
    fi
    
    # 交叉编译工具链配置
    case "${PLATFORM}" in
        linux)
            # Linux ARM64 交叉编译配置
            if [ "$ARCH" = "arm64" ] && [ "$(uname -m)" != "aarch64" ]; then
                echo -e "${YELLOW}检测到 Linux ARM64 交叉编译环境${NC}"
                
                # 检查 ARM64 交叉编译工具链是否安装
                if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
                    echo -e "${GREEN}找到 ARM64 交叉编译工具链${NC}"
                    # 禁用 cairo 的 x11 功能以避免交叉编译时的头文件冲突
                    export VCPKG_CAIRO_FEATURES=""
                    # 注意：不设置全局 CC/CXX 环境变量，避免影响 vcpkg 构建 x64-linux 工具包
                    # 只通过 CMake 参数指定目标架构的编译器
                    # 获取交叉编译工具链的 sysroot 路径
                    SYSROOT=$(aarch64-linux-gnu-gcc -print-sysroot 2>/dev/null || echo "")
                    # 查找 ARM64 系统库路径
                    ARM64_LIB_PATH="/usr/aarch64-linux-gnu/lib"
                    if [ ! -d "$ARM64_LIB_PATH" ]; then
                        # 尝试其他可能的路径
                        ARM64_LIB_PATH=$(find /usr -type d -name "aarch64-linux-gnu" 2>/dev/null | head -1)
                        if [ -n "$ARM64_LIB_PATH" ] && [ -d "$ARM64_LIB_PATH/lib" ]; then
                            ARM64_LIB_PATH="$ARM64_LIB_PATH/lib"
                        else
                            ARM64_LIB_PATH=""
                        fi
                    fi
                    
                    # 动态查找工具链工具的绝对路径
                    AR_TOOL=$(which aarch64-linux-gnu-ar 2>/dev/null || echo "/usr/bin/aarch64-linux-gnu-ar")
                    RANLIB_TOOL=$(which aarch64-linux-gnu-ranlib 2>/dev/null || echo "/usr/bin/aarch64-linux-gnu-ranlib")
                    STRIP_TOOL=$(which aarch64-linux-gnu-strip 2>/dev/null || echo "/usr/bin/aarch64-linux-gnu-strip")
                    CC_TOOL=$(which aarch64-linux-gnu-gcc 2>/dev/null || echo "aarch64-linux-gnu-gcc")
                    CXX_TOOL=$(which aarch64-linux-gnu-g++ 2>/dev/null || echo "aarch64-linux-gnu-g++")
                    
                    CMAKE_ARGS+=(
                        -DCMAKE_SYSTEM_NAME=Linux
                        -DCMAKE_SYSTEM_PROCESSOR=aarch64
                        -DCMAKE_C_COMPILER="$CC_TOOL"
                        -DCMAKE_CXX_COMPILER="$CXX_TOOL"
                        -DCMAKE_AR="$AR_TOOL"
                        -DCMAKE_RANLIB="$RANLIB_TOOL"
                        -DCMAKE_STRIP="$STRIP_TOOL"
                        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
                        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH
                        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH
                        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
                    )
                    # 如果指定了目标 sysroot，优先使用它（用于兼容目标系统的 GLIBC 版本）
                    if [ -n "$TARGET_SYSROOT" ] && [ -d "$TARGET_SYSROOT" ]; then
                        CMAKE_ARGS+=(-DCMAKE_SYSROOT="$TARGET_SYSROOT")
                        echo -e "${BLUE}使用指定的目标 sysroot: $TARGET_SYSROOT${NC}"
                    # 如果找到了工具链的 sysroot，设置它以便查找系统库
                    elif [ -n "$SYSROOT" ] && [ -d "$SYSROOT" ]; then
                        CMAKE_ARGS+=(-DCMAKE_SYSROOT="$SYSROOT")
                        echo -e "${BLUE}使用工具链 sysroot: $SYSROOT${NC}"
                        echo -e "${YELLOW}注意: 如果目标系统 GLIBC 版本较旧，请使用 --sysroot 指定目标系统的 sysroot${NC}"
                    else
                        echo -e "${YELLOW}警告: 未找到 sysroot，二进制可能不兼容目标系统的 GLIBC 版本${NC}"
                        echo -e "${YELLOW}建议: 使用 --sysroot 指定目标系统的 sysroot 路径${NC}"
                    fi
                    
                    # 添加链接器标志以确保兼容性
                    CMAKE_ARGS+=(
                        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--hash-style=both -Wl,--as-needed"
                        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--hash-style=both -Wl,--as-needed"
                    )
                    # 添加 ARM64 系统库路径到库搜索路径和查找根路径
                    if [ -n "$ARM64_LIB_PATH" ] && [ -d "$ARM64_LIB_PATH" ]; then
                        # 设置库路径环境变量，确保 CMake 和链接器能找到系统库
                        export CMAKE_LIBRARY_PATH="$ARM64_LIB_PATH:${CMAKE_LIBRARY_PATH:-}"
                        export CMAKE_PREFIX_PATH="$ARM64_LIB_PATH/..:${CMAKE_PREFIX_PATH:-}"
                        export LD_LIBRARY_PATH="$ARM64_LIB_PATH:${LD_LIBRARY_PATH:-}"
                        CMAKE_ARGS+=(
                            -DCMAKE_LIBRARY_PATH="$ARM64_LIB_PATH"
                            -DCMAKE_PREFIX_PATH="$ARM64_LIB_PATH/.."
                            -DCMAKE_FIND_ROOT_PATH="$ARM64_LIB_PATH/.."
                        )
                        echo -e "${BLUE}添加系统库路径: $ARM64_LIB_PATH${NC}"
                    fi
                else
                    echo -e "${RED}错误: 未找到 ARM64 交叉编译工具链${NC}"
                    echo -e "${YELLOW}请安装交叉编译工具链:${NC}"
                    echo -e "${BLUE}  Ubuntu/Debian: sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu${NC}"
                    echo -e "${BLUE}  Fedora/RHEL:   sudo dnf install gcc-aarch64-linux-gnu gcc-c++-aarch64-linux-gnu${NC}"
                    echo -e "${BLUE}  Arch Linux:    sudo pacman -S aarch64-linux-gnu-gcc${NC}"
                    exit 1
                fi
            fi
            ;;
        android)
            if [ -z "$ANDROID_NDK" ]; then
                echo -e "${YELLOW}警告: ANDROID_NDK 未设置，Android 交叉编译可能需要额外配置${NC}"
            else
                CMAKE_ARGS+=(
                    -DCMAKE_SYSTEM_NAME=Android
                    -DCMAKE_ANDROID_NDK="$ANDROID_NDK"
                    -DCMAKE_ANDROID_ARCH_ABI="$ARCH"
                )
            fi
            ;;
        windows)
            # Windows 交叉编译配置
            if [ "$(uname -s)" != "MINGW"* ] && [ "$(uname -s)" != "MSYS"* ] && [ "$(uname -s)" != "CYGWIN"* ]; then
                # 在 Linux 上交叉编译 Windows 目标
                echo -e "${YELLOW}检测到 Windows 交叉编译环境${NC}"
                
                # 检查 MinGW-w64 是否安装
                if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
                    echo -e "${GREEN}找到 MinGW-w64 交叉编译工具链${NC}"
                    # 设置环境变量，确保 vcpkg 在检测编译器时使用它们
                    export CC=x86_64-w64-mingw32-gcc
                    export CXX=x86_64-w64-mingw32-g++
                    export RC=x86_64-w64-mingw32-windres
                    CMAKE_ARGS+=(
                        -DCMAKE_SYSTEM_NAME=Windows
                        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
                        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
                        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
                    )
                elif command -v mingw-w64-g++ >/dev/null 2>&1; then
                    echo -e "${GREEN}找到 MinGW-w64 交叉编译工具链 (mingw-w64)${NC}"
                    export CC=mingw-w64-gcc
                    export CXX=mingw-w64-g++
                    CMAKE_ARGS+=(
                        -DCMAKE_SYSTEM_NAME=Windows
                        -DCMAKE_C_COMPILER=mingw-w64-gcc
                        -DCMAKE_CXX_COMPILER=mingw-w64-g++
                    )
                else
                    echo -e "${YELLOW}警告: 未找到 MinGW-w64 交叉编译工具链${NC}"
                    echo -e "${YELLOW}请安装: sudo apt-get install mingw-w64${NC}"
                    echo -e "${YELLOW}继续尝试配置，但可能会失败${NC}"
                    CMAKE_ARGS+=(
                        -DCMAKE_SYSTEM_NAME=Windows
                    )
                fi
            fi
            ;;
    esac
    
    # 运行 CMake 配置
    echo -e "${BLUE}运行 CMake 配置命令...${NC}"
    if ! cmake ../.. "${CMAKE_ARGS[@]}"; then
        echo -e "${RED}错误: CMake 配置失败${NC}"
        echo -e "${YELLOW}可能的原因:${NC}"
        echo -e "${BLUE}  1. vcpkg 依赖包下载失败（网络问题）${NC}"
        echo -e "${BLUE}  2. 编译器未正确设置${NC}"
        echo -e "${BLUE}  3. 缺少必要的构建工具${NC}"
        echo ""
        echo -e "${YELLOW}建议:${NC}"
        echo -e "${BLUE}  - 检查网络连接，重试构建${NC}"
        if [ -n "$HTTP_PROXY" ] || [ -n "$HTTPS_PROXY" ]; then
            echo -e "${BLUE}  - 如果代理不可用，尝试使用 --no-proxy 选项:${NC}"
            echo -e "${GREEN}    ./build.sh --no-proxy -a arm64${NC}"
        else
            echo -e "${BLUE}  - 如果使用代理，检查 HTTP_PROXY 和 HTTPS_PROXY 环境变量${NC}"
        fi
        echo -e "${BLUE}  - 查看详细日志: ${BUILD_DIR}/vcpkg-manifest-install.log${NC}"
        cd ../..
        exit 1
    fi
    cd ../..
}

# 编译项目
build_project() {
    echo -e "${BLUE}开始编译...${NC}"
    cd "$BUILD_DIR"
    cmake --build . --config "$BUILD_TYPE" -j "$JOBS"
    cd ../..
    echo -e "${GREEN}编译完成！${NC}"
}

# 显示构建信息
show_build_info() {
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}构建成功！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${BLUE}平台: ${PLATFORM}${NC}"
    echo -e "${BLUE}架构: ${ARCH}${NC}"
    echo -e "${BLUE}构建类型: ${BUILD_TYPE}${NC}"
    echo -e "${BLUE}构建目录: ${BUILD_DIR}${NC}"
    echo ""
    
    # 显示输出文件位置
    if [ "$PLATFORM" = "windows" ]; then
        EXE_EXT=".exe"
        LIB_EXT=".dll"
    else
        EXE_EXT=""
        LIB_EXT=".so"
    fi
    
    if [ -f "${BUILD_DIR}/bin/detector_service${EXE_EXT}" ]; then
        echo -e "${GREEN}可执行文件: ${BUILD_DIR}/bin/detector_service${EXE_EXT}${NC}"
    fi
    
    if [ -d "${BUILD_DIR}/lib" ]; then
        echo -e "${GREEN}库文件目录: ${BUILD_DIR}/lib${NC}"
        echo -e "${BLUE}库文件列表:${NC}"
        ls -lh "${BUILD_DIR}/lib"/*${LIB_EXT} 2>/dev/null || echo "  无库文件"
    fi
    
    echo ""
}

# 主函数
main() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}视觉分析服务构建脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    parse_args "$@"
    
    if [ "$CLEAN" = true ]; then
        clean_build
        exit 0
    fi
    
    check_cmake
    check_proxy
    check_vcpkg
    determine_triplet
    create_build_dir
    configure_cmake
    build_project
    show_build_info
}

# 运行主函数
main "$@"

