#!/bin/bash

# Linaro Ubuntu 20.04 交叉编译构建脚本
# 用于构建 detector_service 项目的 ARM64 版本（针对 Ubuntu 20.04）

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
BUILD_TYPE="Release"
VCPKG_TRIPLET="arm64-linux-linaro-ubuntu20.04"
CLEAN=false
JOBS=$(nproc 2>/dev/null || echo 4)
NO_PROXY=false

# Linaro 工具链配置
LINARO_TOOLCHAIN_PREFIX="${LINARO_TOOLCHAIN_PREFIX:-aarch64-linux-gnu}"
LINARO_TOOLCHAIN_PATH="${LINARO_TOOLCHAIN_PATH:-}"
LINARO_SYSROOT="${LINARO_SYSROOT:-}"

# 显示帮助信息
show_help() {
    cat << EOF
Linaro Ubuntu 20.04 交叉编译构建脚本

用法: $0 [选项]

选项:
    -t, --type TYPE              构建类型 (Debug|Release|RelWithDebInfo) [默认: Release]
    -c, --clean                  清理构建目录
    -j, --jobs N                 并行编译任务数 [默认: CPU核心数]
    --no-proxy                   禁用代理（用于 vcpkg 下载）
    --toolchain-path PATH        Linaro 工具链路径 [默认: 自动检测]
    --sysroot PATH               Ubuntu 20.04 sysroot 路径 [默认: 自动检测]
    --toolchain-prefix PREFIX    工具链前缀 [默认: aarch64-linux-gnu]
    -h, --help                   显示此帮助信息

环境变量:
    LINARO_TOOLCHAIN_PATH        工具链安装路径（例如: /opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu）
    LINARO_SYSROOT                Ubuntu 20.04 sysroot 路径（例如: /opt/ubuntu20.04-rootfs）
    LINARO_TOOLCHAIN_PREFIX      工具链前缀 [默认: aarch64-linux-gnu]

示例:
    $0                                    # 使用默认配置构建
    $0 -t Debug                           # Debug 构建
    $0 --toolchain-path /opt/linaro      # 指定工具链路径
    $0 --sysroot /opt/ubuntu20.04-rootfs # 指定 sysroot 路径
    $0 -c                                 # 清理构建目录

安装 Linaro 工具链:
    1. 下载 Linaro GCC 工具链:
       wget https://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/aarch64-linux-gnu/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu.tar.xz
       tar -xf gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu.tar.xz
       export LINARO_TOOLCHAIN_PATH=\$(pwd)/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu

    2. 创建 Ubuntu 20.04 sysroot（使用 debootstrap）:
       sudo debootstrap --arch=arm64 --foreign focal /opt/ubuntu20.04-rootfs
       sudo chroot /opt/ubuntu20.04-rootfs /debootstrap/debootstrap --second-stage
       export LINARO_SYSROOT=/opt/ubuntu20.04-rootfs

    或者使用现有的交叉编译工具链和 sysroot:
       sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
       sudo apt-get install qemu-user-static binfmt-support
       sudo debootstrap --arch=arm64 --foreign focal /opt/ubuntu20.04-rootfs
       sudo chroot /opt/ubuntu20.04-rootfs /debootstrap/debootstrap --second-stage
EOF
}

# 解析命令行参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
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
            --toolchain-path)
                LINARO_TOOLCHAIN_PATH="$2"
                shift 2
                ;;
            --sysroot)
                LINARO_SYSROOT="$2"
                shift 2
                ;;
            --toolchain-prefix)
                LINARO_TOOLCHAIN_PREFIX="$2"
                shift 2
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

# 检查 CMake
check_cmake() {
    if ! command -v cmake >/dev/null 2>&1; then
        echo -e "${RED}错误: 未找到 CMake${NC}"
        echo -e "${YELLOW}请安装 CMake:${NC}"
        echo -e "${BLUE}  macOS:   brew install cmake${NC}"
        echo -e "${BLUE}  Linux:   sudo apt-get install cmake${NC}"
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
        HTTP_PROXY_VAL="${HTTP_PROXY:-${http_proxy}}"
        HTTPS_PROXY_VAL="${HTTPS_PROXY:-${https_proxy}}"
        ALL_PROXY_VAL="${ALL_PROXY:-${all_proxy}}"
        
        if [ -n "$HTTP_PROXY_VAL" ] || [ -n "$HTTPS_PROXY_VAL" ] || [ -n "$ALL_PROXY_VAL" ]; then
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

# 检测 Linaro 工具链
detect_linaro_toolchain() {
    echo -e "${BLUE}检测 Linaro 工具链...${NC}"
    
    # 如果用户指定了工具链路径，使用它
    if [ -n "$LINARO_TOOLCHAIN_PATH" ]; then
        if [ ! -d "$LINARO_TOOLCHAIN_PATH" ]; then
            echo -e "${RED}错误: 指定的工具链路径不存在: $LINARO_TOOLCHAIN_PATH${NC}"
            exit 1
        fi
        
        CC_TOOL="$LINARO_TOOLCHAIN_PATH/bin/${LINARO_TOOLCHAIN_PREFIX}-gcc"
        CXX_TOOL="$LINARO_TOOLCHAIN_PATH/bin/${LINARO_TOOLCHAIN_PREFIX}-g++"
        AR_TOOL="$LINARO_TOOLCHAIN_PATH/bin/${LINARO_TOOLCHAIN_PREFIX}-ar"
        RANLIB_TOOL="$LINARO_TOOLCHAIN_PATH/bin/${LINARO_TOOLCHAIN_PREFIX}-ranlib"
        STRIP_TOOL="$LINARO_TOOLCHAIN_PATH/bin/${LINARO_TOOLCHAIN_PREFIX}-strip"
        
        if [ ! -f "$CC_TOOL" ] || [ ! -f "$CXX_TOOL" ]; then
            echo -e "${RED}错误: 在指定路径中未找到编译器: $LINARO_TOOLCHAIN_PATH${NC}"
            exit 1
        fi
        
        echo -e "${GREEN}使用指定的 Linaro 工具链: $LINARO_TOOLCHAIN_PATH${NC}"
    else
        # 尝试查找系统安装的工具链
        CC_TOOL=$(which "${LINARO_TOOLCHAIN_PREFIX}-gcc" 2>/dev/null || echo "")
        CXX_TOOL=$(which "${LINARO_TOOLCHAIN_PREFIX}-g++" 2>/dev/null || echo "")
        
        if [ -z "$CC_TOOL" ] || [ -z "$CXX_TOOL" ]; then
            echo -e "${RED}错误: 未找到 Linaro 工具链${NC}"
            echo -e "${YELLOW}请安装交叉编译工具链或设置 LINARO_TOOLCHAIN_PATH 环境变量${NC}"
            echo -e "${BLUE}安装方法:${NC}"
            echo -e "${BLUE}  Ubuntu/Debian: sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu${NC}"
            echo -e "${BLUE}  或下载 Linaro 工具链并设置 LINARO_TOOLCHAIN_PATH${NC}"
            exit 1
        fi
        
        # 获取工具链目录
        LINARO_TOOLCHAIN_PATH=$(dirname $(dirname "$CC_TOOL"))
        AR_TOOL=$(which "${LINARO_TOOLCHAIN_PREFIX}-ar" 2>/dev/null || echo "${LINARO_TOOLCHAIN_PREFIX}-ar")
        RANLIB_TOOL=$(which "${LINARO_TOOLCHAIN_PREFIX}-ranlib" 2>/dev/null || echo "${LINARO_TOOLCHAIN_PREFIX}-ranlib")
        STRIP_TOOL=$(which "${LINARO_TOOLCHAIN_PREFIX}-strip" 2>/dev/null || echo "${LINARO_TOOLCHAIN_PREFIX}-strip")
        
        echo -e "${GREEN}找到系统安装的 Linaro 工具链${NC}"
    fi
    
    # 验证工具链
    echo -e "${BLUE}验证工具链...${NC}"
    "$CC_TOOL" --version | head -n1
    "$CXX_TOOL" --version | head -n1
    
    # 检测 sysroot
    if [ -z "$LINARO_SYSROOT" ]; then
        # 尝试从工具链获取 sysroot
        SYSROOT_FROM_TOOLCHAIN=$("$CC_TOOL" -print-sysroot 2>/dev/null || echo "")
        
        if [ -n "$SYSROOT_FROM_TOOLCHAIN" ] && [ -d "$SYSROOT_FROM_TOOLCHAIN" ]; then
            LINARO_SYSROOT="$SYSROOT_FROM_TOOLCHAIN"
            echo -e "${GREEN}从工具链检测到 sysroot: $LINARO_SYSROOT${NC}"
        else
            # 尝试常见的 sysroot 路径
            COMMON_SYSROOTS=(
                "/usr/${LINARO_TOOLCHAIN_PREFIX}"
                "/usr/lib/${LINARO_TOOLCHAIN_PREFIX}"
                "/opt/ubuntu20.04-rootfs"
                "/opt/ubuntu-rootfs"
            )
            
            for SYSROOT in "${COMMON_SYSROOTS[@]}"; do
                if [ -d "$SYSROOT" ] && [ -d "$SYSROOT/lib" ]; then
                    LINARO_SYSROOT="$SYSROOT"
                    echo -e "${GREEN}找到 sysroot: $LINARO_SYSROOT${NC}"
                    break
                fi
            done
            
            if [ -z "$LINARO_SYSROOT" ]; then
                echo -e "${YELLOW}警告: 未找到 sysroot，某些库可能无法正确链接${NC}"
                echo -e "${BLUE}建议: 创建 Ubuntu 20.04 sysroot 或设置 LINARO_SYSROOT 环境变量${NC}"
            fi
        fi
    else
        if [ ! -d "$LINARO_SYSROOT" ]; then
            echo -e "${RED}错误: 指定的 sysroot 路径不存在: $LINARO_SYSROOT${NC}"
            exit 1
        fi
        echo -e "${GREEN}使用指定的 sysroot: $LINARO_SYSROOT${NC}"
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
    BUILD_DIR="build/linaro-ubuntu20.04-${BUILD_TYPE}"
    mkdir -p "$BUILD_DIR"
    echo -e "${GREEN}构建目录: $BUILD_DIR${NC}"
}

# 检测并设置 CMake 生成器
detect_cmake_generator() {
    if command -v ninja >/dev/null 2>&1; then
        echo -e "${GREEN}使用 Ninja 生成器${NC}" >&2
        echo "Ninja"
    elif command -v make >/dev/null 2>&1; then
        echo -e "${GREEN}使用 Unix Makefiles 生成器${NC}" >&2
        echo "Unix Makefiles"
    else
        echo -e "${RED}错误: 未找到构建工具（ninja 或 make）${NC}" >&2
        exit 1
    fi
}

# 配置 CMake
configure_cmake() {
    echo -e "${BLUE}配置 CMake...${NC}"
    echo -e "${BLUE}构建类型: ${BUILD_TYPE}${NC}"
    echo -e "${BLUE}vcpkg triplet: ${VCPKG_TRIPLET}${NC}"
    echo -e "${BLUE}工具链: ${LINARO_TOOLCHAIN_PREFIX}${NC}"
    echo -e "${BLUE}工具链路径: ${LINARO_TOOLCHAIN_PATH}${NC}"
    if [ -n "$LINARO_SYSROOT" ]; then
        echo -e "${BLUE}sysroot: ${LINARO_SYSROOT}${NC}"
    fi
    
    CMAKE_GENERATOR=$(detect_cmake_generator)
    
    cd "$BUILD_DIR"
    
    CMAKE_ARGS=(
        -G "$CMAKE_GENERATOR"
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
        -DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
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
    
    # 设置 sysroot
    if [ -n "$LINARO_SYSROOT" ]; then
        CMAKE_ARGS+=(-DCMAKE_SYSROOT="$LINARO_SYSROOT")
        CMAKE_ARGS+=(-DCMAKE_FIND_ROOT_PATH="$LINARO_SYSROOT")
        
        # 添加系统库路径
        if [ -d "$LINARO_SYSROOT/lib" ]; then
            CMAKE_ARGS+=(-DCMAKE_LIBRARY_PATH="$LINARO_SYSROOT/lib")
        fi
        if [ -d "$LINARO_SYSROOT/usr/lib" ]; then
            CMAKE_ARGS+=(-DCMAKE_LIBRARY_PATH="$LINARO_SYSROOT/usr/lib:${CMAKE_ARGS[CMAKE_LIBRARY_PATH]}")
        fi
    fi
    
    # 设置 host triplet（用于 vcpkg 构建工具包）
    HOST_ARCH=$(uname -m)
    case "$HOST_ARCH" in
        x86_64)
            export VCPKG_HOST_TRIPLET="x64-linux"
            CMAKE_ARGS+=(-DVCPKG_HOST_TRIPLET="x64-linux")
            export VCPKG_HOST_CC="gcc"
            export VCPKG_HOST_CXX="g++"
            ;;
        aarch64)
            export VCPKG_HOST_TRIPLET="arm64-linux"
            CMAKE_ARGS+=(-DVCPKG_HOST_TRIPLET="arm64-linux")
            ;;
    esac
    
    # 添加 overlay-ports
    if [ -d "$SCRIPT_DIR/ports" ]; then
        CMAKE_ARGS+=(-DVCPKG_OVERLAY_PORTS="$SCRIPT_DIR/ports")
        echo -e "${BLUE}使用 overlay-ports: $SCRIPT_DIR/ports${NC}"
    fi
    
    # 安装自定义 triplet（如果存在）
    if [ -f "$SCRIPT_DIR/triplets/${VCPKG_TRIPLET}.cmake" ]; then
        VCPKG_TRIPLETS_DIR="$VCPKG_ROOT/triplets"
        if [ ! -f "$VCPKG_TRIPLETS_DIR/${VCPKG_TRIPLET}.cmake" ]; then
            echo -e "${BLUE}安装自定义 triplet: ${VCPKG_TRIPLET}.cmake${NC}"
            cp "$SCRIPT_DIR/triplets/${VCPKG_TRIPLET}.cmake" "$VCPKG_TRIPLETS_DIR/${VCPKG_TRIPLET}.cmake"
        fi
    fi
    
    # 设置 vcpkg 环境变量
    export VCPKG_FEATURE_FLAGS=manifests
    export VCPKG_VERBOSE=ON
    export VCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET"
    
    echo -e "${BLUE}运行 CMake 配置命令...${NC}"
    echo -e "${YELLOW}注意: vcpkg 正在安装依赖包，这可能需要较长时间${NC}"
    
    if ! cmake ../.. "${CMAKE_ARGS[@]}" 2>&1 | tee cmake_config.log; then
        echo -e "${RED}错误: CMake 配置失败${NC}"
        echo -e "${YELLOW}查看详细日志: ${BUILD_DIR}/cmake_config.log${NC}"
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
    echo -e "${BLUE}目标平台: Linaro Ubuntu 20.04 (ARM64)${NC}"
    echo -e "${BLUE}构建类型: ${BUILD_TYPE}${NC}"
    echo -e "${BLUE}构建目录: ${BUILD_DIR}${NC}"
    echo ""
    
    if [ -f "${BUILD_DIR}/bin/detector_service" ]; then
        echo -e "${GREEN}可执行文件: ${BUILD_DIR}/bin/detector_service${NC}"
        echo -e "${BLUE}验证二进制文件架构:${NC}"
        file "${BUILD_DIR}/bin/detector_service"
    fi
    
    echo ""
}

# 主函数
main() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Linaro Ubuntu 20.04 交叉编译构建脚本${NC}"
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
    detect_linaro_toolchain
    create_build_dir
    configure_cmake
    build_project
    show_build_info
}

# 运行主函数
main "$@"
