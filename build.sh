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
STATIC_LINK_ALL=false
ENABLE_BM1684=false

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
    --no-proxy                   禁用代理（用于 vcpkg 下载）
    --static                     启用完全静态链接（包括 libc 和 libm，解决 GLIBC 版本问题）
    --bm1684                     启用BM1684平台支持（硬件编解码和TPU推理）
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
    $0 -p linux -a arm64 --static  # Linux ARM64 完全静态链接（解决 GLIBC 版本问题）
    $0 --bm1684                 # 启用BM1684平台支持（硬件编解码和TPU推理）
    $0 -p linux -a arm64 --bm1684  # Linux ARM64 with BM1684 support
    $0 -c                       # 清理构建目录
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
            --no-proxy)
                NO_PROXY=true
                shift
                ;;
            --static)
                STATIC_LINK_ALL=true
                shift
                ;;
            --bm1684)
                ENABLE_BM1684=true
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

# 检查构建工具
check_build_tools() {
    echo -e "${BLUE}检查构建工具...${NC}"
    
    MISSING_TOOLS=()
    
    # 检查 make
    if ! command -v make >/dev/null 2>&1; then
        MISSING_TOOLS+=("make")
    else
        echo -e "${GREEN}找到 make: $(make --version | head -n1)${NC}"
    fi
    
    # 检查编译器（根据平台）
    if [ "$PLATFORM" != "windows" ]; then
        if ! command -v g++ >/dev/null 2>&1; then
            MISSING_TOOLS+=("g++")
        else
            GXX_VERSION=$(g++ --version | head -n1)
            echo -e "${GREEN}找到 g++: $GXX_VERSION${NC}"
        fi
        
        if ! command -v gcc >/dev/null 2>&1; then
            MISSING_TOOLS+=("gcc")
        else
            GCC_VERSION=$(gcc --version | head -n1)
            echo -e "${GREEN}找到 gcc: $GCC_VERSION${NC}"
        fi
    fi
    
    # 如果有缺失的工具，提示安装
    if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
        echo -e "${RED}错误: 缺少以下构建工具: ${MISSING_TOOLS[*]}${NC}"
        echo -e "${YELLOW}请安装构建工具:${NC}"
        if [ "$PLATFORM" = "linux" ]; then
            echo -e "${BLUE}  sudo apt-get update && sudo apt-get install -y build-essential${NC}"
        elif [ "$PLATFORM" = "macos" ]; then
            echo -e "${BLUE}  xcode-select --install${NC}"
        fi
        exit 1
    fi
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

# 检查BM1684 SDK
check_bm1684_sdk() {
    if [ "$ENABLE_BM1684" = true ]; then
        echo -e "${BLUE}检查BM1684 SDK...${NC}"
        
        if [ -z "$BMNNSDK2_TOP" ]; then
            echo -e "${YELLOW}警告: BMNNSDK2_TOP 环境变量未设置${NC}"
            echo -e "${YELLOW}尝试查找BMNNSDK2...${NC}"
            
            # 尝试默认位置
            if [ -d "$HOME/bmnnsdk2/bmnnsdk2-latest" ]; then
                export BMNNSDK2_TOP="$HOME/bmnnsdk2/bmnnsdk2-latest"
                echo -e "${GREEN}找到BMNNSDK2: $BMNNSDK2_TOP${NC}"
            elif [ -d "/opt/bmnnsdk2/bmnnsdk2-latest" ]; then
                export BMNNSDK2_TOP="/opt/bmnnsdk2/bmnnsdk2-latest"
                echo -e "${GREEN}找到BMNNSDK2: $BMNNSDK2_TOP${NC}"
            else
                echo -e "${RED}错误: 未找到BMNNSDK2，请设置 BMNNSDK2_TOP 环境变量${NC}"
                echo -e "${YELLOW}或者将SDK安装在默认位置: $HOME/bmnnsdk2/bmnnsdk2-latest${NC}"
                exit 1
            fi
        fi
        
        if [ ! -d "$BMNNSDK2_TOP" ]; then
            echo -e "${RED}错误: BMNNSDK2目录不存在: $BMNNSDK2_TOP${NC}"
            exit 1
        fi
        
        if [ ! -d "$BMNNSDK2_TOP/include" ]; then
            echo -e "${RED}错误: BMNNSDK2未正确安装，缺少include目录${NC}"
            exit 1
        fi
        
        if [ ! -d "$BMNNSDK2_TOP/lib" ]; then
            echo -e "${RED}错误: BMNNSDK2未正确安装，缺少lib目录${NC}"
            exit 1
        fi
        
        echo -e "${GREEN}使用BMNNSDK2: $BMNNSDK2_TOP${NC}"
        
        # 检查关键库文件
        if [ "$(uname -m)" = "aarch64" ]; then
            LIB_DIR="$BMNNSDK2_TOP/lib/bmnn/soc"
        else
            LIB_DIR="$BMNNSDK2_TOP/lib/bmnn/pcie"
        fi
        
        if [ ! -d "$LIB_DIR" ]; then
            echo -e "${YELLOW}警告: 未找到BMNN库目录: $LIB_DIR${NC}"
            echo -e "${YELLOW}请确认BMNNSDK2安装正确${NC}"
        else
            echo -e "${GREEN}BMNN库目录: $LIB_DIR${NC}"
        fi
    fi
}

# 检查 vcpkg 包安装状态（可选，用于信息提示）
check_vcpkg_packages() {
    if [ -f "$SCRIPT_DIR/vcpkg.json" ] && [ -n "$VCPKG_ROOT" ] && [ -f "$VCPKG_ROOT/vcpkg" ]; then
        echo -e "${BLUE}检查 vcpkg 包安装状态...${NC}"
        # 尝试列出已安装的包（如果 vcpkg 支持）
        if "$VCPKG_ROOT/vcpkg" list --triplet "$VCPKG_TRIPLET" >/dev/null 2>&1; then
            INSTALLED_COUNT=$("$VCPKG_ROOT/vcpkg" list --triplet "$VCPKG_TRIPLET" 2>/dev/null | wc -l)
            if [ "$INSTALLED_COUNT" -gt 0 ]; then
                echo -e "${GREEN}已安装 $INSTALLED_COUNT 个包（triplet: $VCPKG_TRIPLET）${NC}"
            else
                echo -e "${YELLOW}未检测到已安装的包，vcpkg 将在 CMake 配置时自动安装${NC}"
            fi
        fi
    fi
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
    
    # 在交叉编译时，明确指定 host triplet，确保 vcpkg 使用正确的编译器构建工具包
    if [ "$ARCH" = "arm64" ] && [ "$(uname -m)" != "aarch64" ]; then
        # 检测主机架构并设置 host triplet
        HOST_ARCH=$(uname -m)
        case "$HOST_ARCH" in
            x86_64)
                export VCPKG_HOST_TRIPLET="x64-linux"
                CMAKE_ARGS+=(-DVCPKG_HOST_TRIPLET="x64-linux")
                echo -e "${BLUE}设置 vcpkg host triplet: x64-linux（用于构建工具包）${NC}"
                # 保存原生编译器，确保 vcpkg 在构建 host triplet 包时使用它们
                # 这些环境变量会被 vcpkg 用于构建 host tools
                if [ -z "$VCPKG_HOST_CC" ]; then
                    export VCPKG_HOST_CC="gcc"
                    export VCPKG_HOST_CXX="g++"
                    echo -e "${BLUE}设置 host 编译器: gcc/g++（用于构建工具包）${NC}"
                fi
                ;;
            aarch64)
                export VCPKG_HOST_TRIPLET="arm64-linux"
                CMAKE_ARGS+=(-DVCPKG_HOST_TRIPLET="arm64-linux")
                echo -e "${BLUE}设置 vcpkg host triplet: arm64-linux（用于构建工具包）${NC}"
                ;;
            *)
                echo -e "${YELLOW}警告: 未知的主机架构 $HOST_ARCH，vcpkg 将自动检测 host triplet${NC}"
                ;;
        esac
    fi
    
    # 添加 overlay-ports 以使用项目自定义的端口（如 onnxruntime 1.21.0 for GCC 9）
    if [ -d "$SCRIPT_DIR/ports" ]; then
        CMAKE_ARGS+=(-DVCPKG_OVERLAY_PORTS="$SCRIPT_DIR/ports")
        echo -e "${BLUE}使用 overlay-ports: $SCRIPT_DIR/ports${NC}"
    fi
    
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
            # Linux x64 本地编译配置（确保使用本地编译器）
            if [ "$ARCH" = "x64" ] && [ "$(uname -m)" = "x86_64" ]; then
                echo -e "${BLUE}检测到 Linux x64 本地编译环境${NC}"
                # 明确设置使用本地 x64 编译器，避免 vcpkg 误选交叉编译器
                # 清除可能影响编译器检测的环境变量
                unset CC
                unset CXX
                # 确保 vcpkg 使用本地编译器
                export VCPKG_HOST_CC="gcc"
                export VCPKG_HOST_CXX="g++"
                # 明确指定本地编译器路径
                NATIVE_CC=$(which gcc 2>/dev/null || echo "gcc")
                NATIVE_CXX=$(which g++ 2>/dev/null || echo "g++")
                CMAKE_ARGS+=(
                    -DCMAKE_C_COMPILER="$NATIVE_CC"
                    -DCMAKE_CXX_COMPILER="$NATIVE_CXX"
                )
                echo -e "${GREEN}使用本地 x64 编译器: $NATIVE_CC / $NATIVE_CXX${NC}"
            # Linux ARM64 交叉编译配置
            elif [ "$ARCH" = "arm64" ] && [ "$(uname -m)" != "aarch64" ]; then
                echo -e "${YELLOW}检测到 Linux ARM64 交叉编译环境${NC}"
                
                # 检查 ARM64 交叉编译工具链是否安装
                # 检查默认版本或带版本号的编译器（gcc-11, gcc-12, gcc-13 等）
                if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 || \
                   command -v aarch64-linux-gnu-g++-11 >/dev/null 2>&1 || \
                   command -v aarch64-linux-gnu-g++-12 >/dev/null 2>&1 || \
                   command -v aarch64-linux-gnu-g++-13 >/dev/null 2>&1; then
                    echo -e "${GREEN}找到 ARM64 交叉编译工具链${NC}"
                    # 禁用 cairo 的 x11 功能以避免交叉编译时的头文件冲突
                    export VCPKG_CAIRO_FEATURES=""
                    # 注意：不设置全局 CC/CXX 环境变量，避免影响 vcpkg 构建 x64-linux 工具包
                    # 只通过 CMake 参数指定目标架构的编译器
                    # 获取交叉编译工具链的 sysroot 路径
                    # 尝试使用找到的编译器版本获取 sysroot
                    if command -v aarch64-linux-gnu-gcc-11 >/dev/null 2>&1; then
                        SYSROOT=$(aarch64-linux-gnu-gcc-11 -print-sysroot 2>/dev/null || echo "")
                    elif command -v aarch64-linux-gnu-gcc-12 >/dev/null 2>&1; then
                        SYSROOT=$(aarch64-linux-gnu-gcc-12 -print-sysroot 2>/dev/null || echo "")
                    elif command -v aarch64-linux-gnu-gcc-13 >/dev/null 2>&1; then
                        SYSROOT=$(aarch64-linux-gnu-gcc-13 -print-sysroot 2>/dev/null || echo "")
                    else
                        SYSROOT=$(aarch64-linux-gnu-gcc -print-sysroot 2>/dev/null || echo "")
                    fi
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
                    
                    # 动态查找工具链工具
                    # 优先使用 GCC 11 版本（满足 onnxruntime 的 GCC >= 11.1 要求，且与 GLIBC 2.31 兼容）
                    if command -v aarch64-linux-gnu-gcc-11 >/dev/null 2>&1; then
                        CC_TOOL="aarch64-linux-gnu-gcc-11"
                        CXX_TOOL="aarch64-linux-gnu-g++-11"
                        echo -e "${BLUE}使用 GCC 11 版本的交叉编译器（与 GLIBC 2.31 兼容）${NC}"
                    elif command -v aarch64-linux-gnu-gcc-12 >/dev/null 2>&1; then
                        CC_TOOL="aarch64-linux-gnu-gcc-12"
                        CXX_TOOL="aarch64-linux-gnu-g++-12"
                        echo -e "${YELLOW}使用 GCC 12 版本（可能需要较新的 GLIBC）${NC}"
                    elif command -v aarch64-linux-gnu-gcc-13 >/dev/null 2>&1; then
                        CC_TOOL="aarch64-linux-gnu-gcc-13"
                        CXX_TOOL="aarch64-linux-gnu-g++-13"
                        echo -e "${YELLOW}使用 GCC 13 版本（需要 GLIBC 2.34+，可能不兼容）${NC}"
                    else
                        CC_TOOL=$(which aarch64-linux-gnu-gcc 2>/dev/null || echo "aarch64-linux-gnu-gcc")
                        CXX_TOOL=$(which aarch64-linux-gnu-g++ 2>/dev/null || echo "aarch64-linux-gnu-g++")
                        echo -e "${BLUE}使用默认交叉编译器: $CC_TOOL${NC}"
                    fi
                    AR_TOOL=$(which aarch64-linux-gnu-ar 2>/dev/null || echo "aarch64-linux-gnu-ar")
                    RANLIB_TOOL=$(which aarch64-linux-gnu-ranlib 2>/dev/null || echo "aarch64-linux-gnu-ranlib")
                    STRIP_TOOL=$(which aarch64-linux-gnu-strip 2>/dev/null || echo "aarch64-linux-gnu-strip")
                    
                    # 保存原始的 CC/CXX 环境变量（用于 vcpkg 构建 host tools）
                    # 确保 vcpkg 在构建 x64-linux 工具包时使用本地编译器
                    # 注意：不要设置全局 CC/CXX，因为它们会影响 vcpkg 构建 host triplet 包
                    # 而是通过 VCPKG_HOST_CC/VCPKG_HOST_CXX 来指定 host 编译器
                    if [ -z "$VCPKG_ORIGINAL_CC" ]; then
                        export VCPKG_ORIGINAL_CC="${CC:-gcc}"
                        export VCPKG_ORIGINAL_CXX="${CXX:-g++}"
                    fi
                    # 确保 vcpkg 在构建 host triplet 包时使用原生编译器
                    # 通过环境变量明确指定，避免被交叉编译器覆盖
                    if [ -z "$VCPKG_HOST_CC" ] && [ "$(uname -m)" = "x86_64" ]; then
                        export VCPKG_HOST_CC="gcc"
                        export VCPKG_HOST_CXX="g++"
                    fi
                    
                    # 保存原生编译器路径，用于 vcpkg 构建 host triplet 包
                    NATIVE_CC=$(which gcc 2>/dev/null || echo "gcc")
                    NATIVE_CXX=$(which g++ 2>/dev/null || echo "g++")
                    
                    # 设置交叉编译器的环境变量，确保 vcpkg 在 detect_compiler 阶段能找到编译器
                    # vcpkg 在检测编译器时会读取这些环境变量
                    export CMAKE_C_COMPILER="$CC_TOOL"
                    export CMAKE_CXX_COMPILER="$CXX_TOOL"
                    export CMAKE_AR="$AR_TOOL"
                    export CMAKE_RANLIB="$RANLIB_TOOL"
                    export CMAKE_STRIP="$STRIP_TOOL"
                    
                    # 注意：不要设置全局 CC/CXX 环境变量，因为它们会影响 vcpkg 检测 host triplet
                    # 只设置 CMAKE_C_COMPILER 等 CMake 特定的环境变量
                    # vcpkg 在检测 target triplet 时会读取 CMAKE_C_COMPILER 环境变量
                    export CMAKE_C_COMPILER="$CC_TOOL"
                    export CMAKE_CXX_COMPILER="$CXX_TOOL"
                    export CMAKE_AR="$AR_TOOL"
                    export CMAKE_RANLIB="$RANLIB_TOOL"
                    export CMAKE_STRIP="$STRIP_TOOL"
                    
                    # 编译器名称已在 PATH 中，不需要额外添加目录
                    echo -e "${BLUE}设置交叉编译器环境变量: CMAKE_C_COMPILER=$CMAKE_C_COMPILER, CMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER${NC}"
                    
                    # 设置交叉编译器的 CMake 变量
                    # 注意：这些变量只影响目标架构的包，不影响 host triplet 包
                    # 但为了确保 vcpkg 在构建 host triplet 包时使用原生编译器，
                    # 我们需要通过环境变量明确指定
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
                    
                    # 为 vcpkg 设置环境变量，确保构建 host triplet 包时使用原生编译器
                    # 这些环境变量会被 vcpkg 传递给构建 host tools 的构建系统
                    export VCPKG_HOST_C_COMPILER="$NATIVE_CC"
                    export VCPKG_HOST_CXX_COMPILER="$NATIVE_CXX"
                    # 同时设置 CC/CXX 环境变量，确保 meson 等构建系统在构建 host triplet 包时使用原生编译器
                    # 注意：这些变量只影响 vcpkg 构建 host tools，不影响目标架构的包
                    # 因为 vcpkg 在构建 host tools 时会使用这些环境变量
                    export VCPKG_HOST_CC="$NATIVE_CC"
                    export VCPKG_HOST_CXX="$NATIVE_CXX"
                    # 如果找到了 sysroot，设置它以便查找系统库
                    if [ -n "$SYSROOT" ] && [ -d "$SYSROOT" ]; then
                        CMAKE_ARGS+=(-DCMAKE_SYSROOT="$SYSROOT")
                        echo -e "${BLUE}使用 sysroot: $SYSROOT${NC}"
                    fi
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
    
    # 检查 vcpkg 包是否已安装（可选，用于提前了解状态）
    if [ -f "../../vcpkg.json" ]; then
        echo -e "${BLUE}检测到 vcpkg.json，vcpkg 将在 CMake 配置时自动安装依赖包${NC}"
        echo -e "${YELLOW}注意: 在 ARM64 平台上，某些大型包（如 ffmpeg、opencv4）可能需要较长时间编译${NC}"
        echo -e "${YELLOW}这可能需要数小时，请耐心等待...${NC}"
        echo ""
        echo -e "${BLUE}提示: 如果想查看 vcpkg 安装进度，可以在另一个终端运行:${NC}"
        echo -e "${GREEN}  tail -f ${BUILD_DIR}/vcpkg-manifest-install.log${NC}"
        echo ""
    fi
    
    # 运行 CMake 配置（添加详细输出）
    echo -e "${BLUE}运行 CMake 配置命令...${NC}"
    echo -e "${BLUE}（vcpkg 正在安装依赖包，这可能需要较长时间，请耐心等待）${NC}"
    
    # 设置 vcpkg 详细输出环境变量
    export VCPKG_FEATURE_FLAGS=manifests
    export VCPKG_VERBOSE=ON
    # 设置 vcpkg triplet 环境变量，确保在重新配置时也能使用
    export VCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET"
    
    # 对于 x64-linux 构建，确保 vcpkg 使用本地编译器
    # 防止 vcpkg 误选交叉编译器（如 ARM64 交叉编译器）
    if [ "$ARCH" = "x64" ] && [ "$(uname -m)" = "x86_64" ] && [ "$VCPKG_TRIPLET" = "x64-linux" ]; then
        # 明确设置 vcpkg 使用本地编译器
        export VCPKG_HOST_CC="gcc"
        export VCPKG_HOST_CXX="g++"
        # 确保 CC/CXX 环境变量指向本地编译器（如果已设置）
        if [ -n "$CC" ] && echo "$CC" | grep -q "aarch64"; then
            echo -e "${YELLOW}警告: 检测到 CC 环境变量指向 ARM64 交叉编译器，已清除${NC}"
            unset CC
        fi
        if [ -n "$CXX" ] && echo "$CXX" | grep -q "aarch64"; then
            echo -e "${YELLOW}警告: 检测到 CXX 环境变量指向 ARM64 交叉编译器，已清除${NC}"
            unset CXX
        fi
        # 设置 vcpkg 编译器检测环境变量，强制使用本地编译器
        export VCPKG_C_COMPILER="gcc"
        export VCPKG_CXX_COMPILER="g++"
        echo -e "${BLUE}强制 vcpkg 使用本地 x64 编译器进行构建${NC}"
    fi
    
    # 在交叉编译时，确保 vcpkg 构建 host tools 时使用本地编译器
    # 这很重要，因为 vcpkg 需要构建一些工具（如 pkgconf）在主机上运行
    if [ "$ARCH" = "arm64" ] && [ "$(uname -m)" != "aarch64" ]; then
        # 确保 vcpkg 在构建 host tools 时使用本地编译器
        # 清除可能影响 host triplet 检测的环境变量
        if [ -n "$CC" ] && echo "$CC" | grep -q "aarch64"; then
            echo -e "${YELLOW}警告: 检测到 CC 环境变量指向 ARM64 交叉编译器，临时清除以检测 host triplet${NC}"
            unset CC
        fi
        if [ -n "$CXX" ] && echo "$CXX" | grep -q "aarch64"; then
            echo -e "${YELLOW}警告: 检测到 CXX 环境变量指向 ARM64 交叉编译器，临时清除以检测 host triplet${NC}"
            unset CXX
        fi
        # 明确设置 host 编译器环境变量
        HOST_ARCH=$(uname -m)
        if [ "$HOST_ARCH" = "x86_64" ]; then
            export VCPKG_HOST_CC="gcc"
            export VCPKG_HOST_CXX="g++"
            echo -e "${BLUE}设置 VCPKG_HOST_CC/VCPKG_HOST_CXX 为本地编译器（用于 host triplet）${NC}"
        fi
    fi
    
    # 添加静态链接选项（如果启用）
    if [ "$STATIC_LINK_ALL" = true ]; then
        CMAKE_ARGS+=(-DSTATIC_LINK_ALL=ON)
        echo -e "${GREEN}启用完全静态链接（包括 libc 和 libm）${NC}"
        echo -e "${YELLOW}注意: 完全静态链接会产生较大的可执行文件，但可以解决 GLIBC 版本不匹配问题${NC}"
    else
        echo -e "${BLUE}使用部分静态链接（libgcc 和 libstdc++ 静态链接，系统库动态链接）${NC}"
        echo -e "${BLUE}提示: 如需完全静态链接，请使用 --static 选项${NC}"
    fi
    
    # 添加BM1684支持选项（如果启用）
    if [ "$ENABLE_BM1684" = true ]; then
        CMAKE_ARGS+=(-DENABLE_BM1684=ON)
        echo -e "${GREEN}启用BM1684平台支持（硬件编解码和TPU推理）${NC}"
        # 设置BMNNSDK2_TOP环境变量，确保CMake能找到SDK
        if [ -n "$BMNNSDK2_TOP" ]; then
            export BMNNSDK2_TOP="$BMNNSDK2_TOP"
            echo -e "${BLUE}BMNNSDK2_TOP: $BMNNSDK2_TOP${NC}"
        fi
    else
        echo -e "${BLUE}BM1684平台支持已禁用${NC}"
        echo -e "${BLUE}提示: 如需启用BM1684支持，请使用 --bm1684 选项${NC}"
    fi
    
    # 运行 CMake，并将输出同时显示和保存到日志
    if ! cmake ../.. "${CMAKE_ARGS[@]}" 2>&1 | tee cmake_config.log; then
        echo -e "${RED}错误: CMake 配置失败${NC}"
        echo ""
        
        # 检查是否是网络下载失败
        if grep -q "Download failed\|vcpkg_download_distfile\|vcpkg_from_github" cmake_config.log 2>/dev/null; then
            echo -e "${YELLOW}检测到网络下载失败问题${NC}"
            echo -e "${BLUE}可能的原因:${NC}"
            echo -e "${BLUE}  1. 网络连接问题（无法访问 GitHub 或其他下载源）${NC}"
            echo -e "${BLUE}  2. 代理设置错误（代理地址、端口或认证信息不正确）${NC}"
            echo -e "${BLUE}  3. 代理服务器故障或不可用${NC}"
            echo -e "${BLUE}  4. 防火墙阻止了连接${NC}"
            echo ""
            echo -e "${YELLOW}解决方案:${NC}"
            if [ -n "$HTTP_PROXY" ] || [ -n "$HTTPS_PROXY" ]; then
                echo -e "${BLUE}  1. 如果代理不可用，尝试禁用代理:${NC}"
                echo -e "${GREEN}     ./build.sh --no-proxy -a arm64${NC}"
                echo -e "${BLUE}  2. 检查代理设置是否正确:${NC}"
                echo -e "${GREEN}     echo \$HTTP_PROXY \$HTTPS_PROXY${NC}"
                echo -e "${BLUE}  3. 测试代理连接:${NC}"
                echo -e "${GREEN}     curl -I https://github.com${NC}"
            else
                echo -e "${BLUE}  1. 如果使用代理，设置代理环境变量:${NC}"
                echo -e "${GREEN}     export HTTP_PROXY=http://proxy.example.com:8080${NC}"
                echo -e "${GREEN}     export HTTPS_PROXY=http://proxy.example.com:8080${NC}"
                echo -e "${BLUE}  2. 或者尝试使用 --no-proxy 选项:${NC}"
                echo -e "${GREEN}     ./build.sh --no-proxy -a arm64${NC}"
            fi
            echo -e "${BLUE}  3. 检查网络连接:${NC}"
            echo -e "${GREEN}     ping github.com${NC}"
            echo -e "${BLUE}  4. 查看 vcpkg 详细错误日志:${NC}"
            if [ -f "${BUILD_DIR}/vcpkg-manifest-install.log" ]; then
                echo -e "${GREEN}     tail -n 50 ${BUILD_DIR}/vcpkg-manifest-install.log${NC}"
            fi
        # 检查是否是缺少构建工具
        elif grep -q "CMAKE_MAKE_PROGRAM is not set\|unable to find a build program\|make.*not found" cmake_config.log 2>/dev/null; then
            echo -e "${YELLOW}检测到缺少构建工具问题${NC}"
            echo -e "${BLUE}CMake 无法找到 make 工具${NC}"
            echo ""
            echo -e "${YELLOW}解决方案:${NC}"
            if [ "$PLATFORM" = "linux" ]; then
                echo -e "${BLUE}  安装构建工具:${NC}"
                echo -e "${GREEN}    sudo apt-get update && sudo apt-get install -y build-essential${NC}"
            elif [ "$PLATFORM" = "macos" ]; then
                echo -e "${BLUE}  安装 Xcode 命令行工具:${NC}"
                echo -e "${GREEN}    xcode-select --install${NC}"
            fi
        else
            echo -e "${YELLOW}可能的原因:${NC}"
            echo -e "${BLUE}  1. vcpkg 依赖包下载/编译失败（网络问题或编译错误）${NC}"
            echo -e "${BLUE}  2. 编译器未正确设置${NC}"
            echo -e "${BLUE}  3. 缺少必要的构建工具${NC}"
            echo -e "${BLUE}  4. 内存不足（某些包编译需要大量内存）${NC}"
            echo ""
            echo -e "${YELLOW}建议:${NC}"
            echo -e "${BLUE}  - 检查网络连接，重试构建${NC}"
            if [ -n "$HTTP_PROXY" ] || [ -n "$HTTPS_PROXY" ]; then
                echo -e "${BLUE}  - 如果代理不可用，尝试使用 --no-proxy 选项:${NC}"
                echo -e "${GREEN}    ./build.sh --no-proxy -a arm64${NC}"
            else
                echo -e "${BLUE}  - 如果使用代理，检查 HTTP_PROXY 和 HTTPS_PROXY 环境变量${NC}"
            fi
        fi
        
        echo ""
        echo -e "${BLUE}查看详细日志:${NC}"
        echo -e "${BLUE}  - CMake 配置日志: ${BUILD_DIR}/cmake_config.log${NC}"
        if [ -f "${BUILD_DIR}/vcpkg-manifest-install.log" ]; then
            echo -e "${BLUE}  - vcpkg 安装日志: ${BUILD_DIR}/vcpkg-manifest-install.log${NC}"
        fi
        echo -e "${BLUE}  - 如果 vcpkg 安装卡住，可以尝试手动安装依赖:${NC}"
        echo -e "${GREEN}    cd $VCPKG_ROOT && ./vcpkg install --triplet ${VCPKG_TRIPLET} --x-manifest-root=../../${NC}"
        cd ../..
        exit 1
    fi
    cd ../..
}

# 编译项目
build_project() {
    echo -e "${BLUE}开始编译...${NC}"
    cd "$BUILD_DIR"
    
    if ! cmake --build . --config "$BUILD_TYPE" -j "$JOBS"; then
        echo -e "${RED}错误: 编译失败${NC}"
        echo -e "${YELLOW}可能的原因:${NC}"
        echo -e "${BLUE}  1. 源代码编译错误${NC}"
        echo -e "${BLUE}  2. 缺少必要的头文件或库文件${NC}"
        echo -e "${BLUE}  3. 内存不足${NC}"
        echo -e "${BLUE}  4. 编译器版本不兼容${NC}"
        echo ""
        echo -e "${YELLOW}建议:${NC}"
        echo -e "${BLUE}  - 查看编译错误信息，修复源代码问题${NC}"
        echo -e "${BLUE}  - 检查是否所有依赖都已正确安装${NC}"
        echo -e "${BLUE}  - 尝试减少并行编译任务数: -j 4${NC}"
        cd ../..
        exit 1
    fi
    
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
    check_build_tools
    check_proxy
    check_vcpkg
    check_bm1684_sdk
    determine_triplet
    check_vcpkg_packages
    create_build_dir
    configure_cmake
    build_project
    show_build_info
}

# 运行主函数
main "$@"

