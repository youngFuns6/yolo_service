#!/bin/bash

# BM1684 ARM64 编译脚本
# 使用方法: ./build-bm1684-arm64.sh [build_dir]

set -e

# 设置 SophonSDK 路径（通过环境变量 REL_TOP 或 BMNNSDK2_TOP）
# 在 Docker 编译环境中，SDK 路径通常已通过环境变量设置
if [ -z "$REL_TOP" ] && [ -z "$BMNNSDK2_TOP" ]; then
    echo "错误: 请设置环境变量 REL_TOP 或 BMNNSDK2_TOP 指向 SophonSDK 根目录"
    echo "在 Docker 编译环境中，SDK 路径通常已通过环境变量设置"
    echo "例如: export REL_TOP=/path/to/sophonsdk_v3.0.0"
    exit 1
fi

# 设置构建目录
BUILD_DIR=${1:-"build/bm1684-arm64-Release"}
echo "构建目录: $BUILD_DIR"

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 设置 CMake 参数
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DENABLE_BM1684=ON
    -DUSE_BM_FFMPEG=ON
    -DUSE_BM_OPENCV=ON
    -DTARGET_ARCH=arm64
)

# 如果设置了 vcpkg，添加工具链文件
if [ -n "$VCPKG_ROOT" ] && [ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
    CMAKE_ARGS+=(
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
        -DVCPKG_TARGET_TRIPLET=arm64-linux
    )
    echo "使用 vcpkg 工具链: $VCPKG_ROOT"
fi

# 如果设置了交叉编译工具链
if [ -n "$CROSS_COMPILE_PREFIX" ]; then
    CMAKE_ARGS+=(
        -DCMAKE_C_COMPILER="${CROSS_COMPILE_PREFIX}gcc"
        -DCMAKE_CXX_COMPILER="${CROSS_COMPILE_PREFIX}g++"
    )
    echo "使用交叉编译器: $CROSS_COMPILE_PREFIX"
fi

# 运行 CMake 配置
echo "配置 CMake..."
cmake ../.. "${CMAKE_ARGS[@]}"

# 编译
echo "开始编译..."
cmake --build . -j$(nproc)

echo "编译完成！"
echo "可执行文件位置: $BUILD_DIR/bin/detector_service"
