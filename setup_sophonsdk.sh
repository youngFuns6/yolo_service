#!/bin/bash

# SophonSDK 解压和设置脚本
# 将 examples/sophonsdk_v3.0.0_20220716/sophonsdk_v3.0.0.tar.gz 解压到指定目录

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_ARCHIVE="${SCRIPT_DIR}/examples/sophonsdk_v3.0.0_20220716/sophonsdk_v3.0.0.tar.gz"
SDK_EXTRACT_DIR="${SCRIPT_DIR}/third_party"

# 检查压缩包是否存在
if [ ! -f "$SDK_ARCHIVE" ]; then
    echo "错误: 找不到 SophonSDK 压缩包: $SDK_ARCHIVE"
    exit 1
fi

# 创建解压目录
mkdir -p "$SDK_EXTRACT_DIR"

# 检查是否已经解压
SDK_DIR="${SDK_EXTRACT_DIR}/sophonsdk_v3.0.0"
if [ -d "$SDK_DIR" ]; then
    echo "SophonSDK 已经解压到: $SDK_DIR"
    echo "如果要重新解压，请先删除该目录"
    exit 0
fi

echo "正在解压 SophonSDK..."
echo "源文件: $SDK_ARCHIVE"
echo "目标目录: $SDK_EXTRACT_DIR"

# 解压到临时目录
TEMP_DIR="${SDK_EXTRACT_DIR}/sophonsdk_temp"
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# 解压
tar -xzf "$SDK_ARCHIVE"

# 查找解压后的目录（通常是 sophonsdk_v3.0.0_xxxxx 格式）
EXTRACTED_DIR=$(find . -maxdepth 1 -type d -name "sophonsdk*" | head -1)

if [ -z "$EXTRACTED_DIR" ]; then
    echo "错误: 无法找到解压后的目录"
    exit 1
fi

# 移动到目标位置
mv "$EXTRACTED_DIR" "$SDK_DIR"
cd "$SCRIPT_DIR"
rmdir "$TEMP_DIR" 2>/dev/null || true

echo "SophonSDK 解压完成！"
echo "SDK 路径: $SDK_DIR"

# 检查 SDK 结构
if [ -d "$SDK_DIR/include" ] && [ -d "$SDK_DIR/lib" ]; then
    echo "SDK 结构验证成功"
    echo ""
    echo "请设置环境变量:"
    echo "  export REL_TOP=$SDK_DIR"
    echo "或者"
    echo "  export BMNNSDK2_TOP=$SDK_DIR"
else
    echo "警告: SDK 结构可能不完整，请检查 $SDK_DIR"
fi
