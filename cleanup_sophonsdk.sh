#!/bin/bash

# SophonSDK 清理脚本
# 只保留编译和运行所需的必要文件，删除工具、测试、固件等

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="${SCRIPT_DIR}/third_party/sophonsdk_v3.0.0"

# 配置选项（可以通过参数控制）
KEEP_FFMPEG=${1:-"yes"}  # 是否保留 FFmpeg（默认保留）
KEEP_OPENCV=${2:-"yes"}   # 是否保留 OpenCV（默认保留）

if [ ! -d "$SDK_DIR" ]; then
    echo "错误: SophonSDK 目录不存在: $SDK_DIR"
    exit 1
fi

echo "=========================================="
echo "SophonSDK 清理脚本"
echo "=========================================="
echo "SDK 目录: $SDK_DIR"
echo "保留 FFmpeg: $KEEP_FFMPEG"
echo "保留 OpenCV: $KEEP_OPENCV"
echo ""

# 计算清理前的大小
BEFORE_SIZE=$(du -sh "$SDK_DIR" | cut -f1)
echo "清理前大小: $BEFORE_SIZE"

# 需要保留的目录和文件
KEEP_DIRS=(
    "include"           # 头文件（必需）
    "lib"              # 库文件（必需）
    "release_version.txt"  # 版本信息
)

# 可以删除的目录（工具、测试、固件等）
REMOVE_DIRS=(
    "bin"              # 工具程序
    "scripts"          # 脚本
    "driver"           # 驱动（运行时不需要）
    "install"          # 安装脚本
    "res"              # 资源文件
    "test"             # 测试文件
    "bmnet"            # 模型转换工具（编译时不需要）
    "bmlang"           # 语言工具
    "bm1684x_firmware" # 固件（如果需要可以保留，但通常不需要）
    "bm1684_firmware"  # 固件（如果需要可以保留，但通常不需要）
)

# 确认删除
echo "将删除以下目录:"
for dir in "${REMOVE_DIRS[@]}"; do
    if [ -d "$SDK_DIR/$dir" ]; then
        SIZE=$(du -sh "$SDK_DIR/$dir" 2>/dev/null | cut -f1)
        echo "  - $dir ($SIZE)"
    fi
done

echo ""
read -p "确认删除? (y/N): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "取消清理"
    exit 0
fi

# 删除不需要的目录
echo "正在删除不需要的目录..."
for dir in "${REMOVE_DIRS[@]}"; do
    if [ -d "$SDK_DIR/$dir" ]; then
        echo "删除: $dir"
        rm -rf "$SDK_DIR/$dir"
    fi
done

# 清理 lib 目录中不需要的子目录
echo ""
echo "清理 lib 目录中不需要的子目录..."

# 保留的 lib 子目录（必需）
LIB_KEEP_DIRS=(
    "bmnn"         # 核心推理库（必需）- 包含 bmrt, bmcv, bmlib
    "sail"         # Sail 推理库（推荐）- 高级推理接口
    "thirdparty"   # 第三方依赖库（必需）- boost, protobuf, gflags, glog
)

# 根据配置决定是否保留
if [ "$KEEP_FFMPEG" = "yes" ]; then
    LIB_KEEP_DIRS+=("ffmpeg" "decode")
fi

if [ "$KEEP_OPENCV" = "yes" ]; then
    LIB_KEEP_DIRS+=("opencv")
fi

# 可以删除的 lib 子目录
LIB_REMOVE_DIRS=(
    "bmcompiler"       # 模型编译工具（编译时不需要）
    "bmlang"           # 语言工具（不需要）
    "calibration-tools" # 校准工具（不需要）
    "sys"              # 系统库（通常不需要）
    "vpp"              # 视频处理（如果不需要可以删除）
)

# 删除 lib 中不需要的子目录
for dir in "${LIB_REMOVE_DIRS[@]}"; do
    if [ -d "$SDK_DIR/lib/$dir" ]; then
        SIZE=$(du -sh "$SDK_DIR/lib/$dir" 2>/dev/null | cut -f1)
        echo "删除 lib/$dir ($SIZE)"
        rm -rf "$SDK_DIR/lib/$dir"
    fi
done

# 验证保留的目录
echo ""
echo "保留的 lib 子目录:"
for keep_dir in "${LIB_KEEP_DIRS[@]}"; do
    if [ -d "$SDK_DIR/lib/$keep_dir" ]; then
        SIZE=$(du -sh "$SDK_DIR/lib/$keep_dir" 2>/dev/null | cut -f1)
        echo "  ✓ lib/$keep_dir ($SIZE)"
    fi
done

# 清理 include 目录中不需要的子目录
echo ""
echo "清理 include 目录中不需要的子目录..."

# 保留的 include 子目录（必需）
INCLUDE_KEEP_DIRS=(
    "bmruntime"    # BMRuntime 头文件（必需）
    "bmlib"        # BMLib 头文件（必需）
    "sail"         # Sail 头文件（推荐）
    "third_party"  # 第三方头文件（必需）
)

# 根据配置决定是否保留
if [ "$KEEP_FFMPEG" = "yes" ]; then
    INCLUDE_KEEP_DIRS+=("ffmpeg" "decode")
fi

if [ "$KEEP_OPENCV" = "yes" ]; then
    INCLUDE_KEEP_DIRS+=("opencv")
fi

# 可以删除的 include 子目录
INCLUDE_REMOVE_DIRS=(
    "bmcompiler"       # 编译工具头文件
    "bmlang"           # 语言工具头文件
    "bmcpu"            # CPU 相关（可能不需要）
    "bmlog"            # 日志（可能不需要）
    "bmnetc"           # 模型转换工具
    "bmnetu"           # 模型转换工具
    "calibration"      # 校准工具
    "ufw"              # UFW（可能不需要）
    "vpp"              # 视频处理（如果不需要）
)

# 删除 include 中不需要的子目录
for dir in "${INCLUDE_REMOVE_DIRS[@]}"; do
    if [ -d "$SDK_DIR/include/$dir" ]; then
        SIZE=$(du -sh "$SDK_DIR/include/$dir" 2>/dev/null | cut -f1)
        echo "删除 include/$dir ($SIZE)"
        rm -rf "$SDK_DIR/include/$dir"
    fi
done

echo ""
echo "保留的 include 子目录:"
for keep_dir in "${INCLUDE_KEEP_DIRS[@]}"; do
    if [ -d "$SDK_DIR/include/$keep_dir" ]; then
        SIZE=$(du -sh "$SDK_DIR/include/$keep_dir" 2>/dev/null | cut -f1)
        echo "  ✓ include/$keep_dir ($SIZE)"
    fi
done

# 计算清理后的大小
AFTER_SIZE=$(du -sh "$SDK_DIR" | cut -f1)
echo ""
echo "清理完成！"
echo "清理前大小: $BEFORE_SIZE"
echo "清理后大小: $AFTER_SIZE"

# 显示保留的目录结构
echo ""
echo "=========================================="
echo "清理完成！保留的目录结构:"
echo "=========================================="
echo ""
echo "根目录:"
ls -d "$SDK_DIR"/*/ 2>/dev/null | xargs -n1 basename | while read dir; do
    SIZE=$(du -sh "$SDK_DIR/$dir" 2>/dev/null | cut -f1)
    echo "  - $dir ($SIZE)"
done

echo ""
echo "lib/ 子目录:"
ls -d "$SDK_DIR/lib"/*/ 2>/dev/null | xargs -n1 basename | while read dir; do
    SIZE=$(du -sh "$SDK_DIR/lib/$dir" 2>/dev/null | cut -f1)
    echo "  - lib/$dir ($SIZE)"
done

echo ""
echo "include/ 子目录:"
ls -d "$SDK_DIR/include"/*/ 2>/dev/null | xargs -n1 basename | while read dir; do
    SIZE=$(du -sh "$SDK_DIR/include/$dir" 2>/dev/null | cut -f1)
    echo "  - include/$dir ($SIZE)"
done

echo ""
echo "=========================================="
echo "使用说明:"
echo "=========================================="
echo "最小配置（只保留核心库）:"
echo "  ./cleanup_sophonsdk.sh no no"
echo ""
echo "保留 FFmpeg（硬件编解码）:"
echo "  ./cleanup_sophonsdk.sh yes no"
echo ""
echo "保留所有（默认）:"
echo "  ./cleanup_sophonsdk.sh yes yes"
echo "  或"
echo "  ./cleanup_sophonsdk.sh"
