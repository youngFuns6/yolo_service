#!/bin/bash

# 脚本：应用 ONNX schema 重复注册修复 patch
# 使用方法: ./apply_onnx_patch.sh

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}应用 ONNX Schema 重复注册修复 Patch${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查 VCPKG_ROOT 环境变量
if [ -z "$VCPKG_ROOT" ]; then
    echo -e "${RED}错误: VCPKG_ROOT 环境变量未设置${NC}"
    echo -e "${YELLOW}请设置 VCPKG_ROOT 环境变量，例如:${NC}"
    echo -e "${GREEN}  export VCPKG_ROOT=/path/to/vcpkg${NC}"
    exit 1
fi

if [ ! -d "$VCPKG_ROOT" ]; then
    echo -e "${RED}错误: VCPKG_ROOT 目录不存在: $VCPKG_ROOT${NC}"
    exit 1
fi

echo -e "${GREEN}✓ VCPKG_ROOT: $VCPKG_ROOT${NC}"

# 检查项目目录
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo -e "${GREEN}✓ 项目目录: $PROJECT_DIR${NC}"

# 检查 patch 文件是否存在
PATCH_DIR="$PROJECT_DIR/ports/onnx"
if [ ! -f "$PATCH_DIR/fix-schema-duplicate.patch" ]; then
    echo -e "${RED}错误: 找不到 patch 文件: $PATCH_DIR/fix-schema-duplicate.patch${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Patch 文件存在${NC}"
echo ""

# 检查当前 triplet
if [ -z "$VCPKG_TARGET_TRIPLET" ]; then
    # 尝试推断 triplet
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if [[ $(uname -m) == "arm64" ]]; then
            VCPKG_TARGET_TRIPLET="arm64-osx"
        else
            VCPKG_TARGET_TRIPLET="x64-osx"
        fi
    else
        VCPKG_TARGET_TRIPLET="x64-linux"
    fi
    echo -e "${YELLOW}未设置 VCPKG_TARGET_TRIPLET，使用默认值: $VCPKG_TARGET_TRIPLET${NC}"
fi

echo -e "${BLUE}使用 triplet: $VCPKG_TARGET_TRIPLET${NC}"
echo ""

# 确认操作
echo -e "${YELLOW}此操作将:${NC}"
echo -e "${BLUE}  1. 移除已安装的 onnxruntime 和 onnx${NC}"
echo -e "${BLUE}  2. 使用 overlay-ports 重新安装 onnx（应用 patch）${NC}"
echo -e "${BLUE}  3. 重新安装 onnxruntime${NC}"
echo ""
read -p "是否继续? (y/N): " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}操作已取消${NC}"
    exit 0
fi

echo ""
echo -e "${BLUE}步骤 1: 移除已安装的包...${NC}"
cd "$VCPKG_ROOT"
./vcpkg remove onnxruntime --triplet "$VCPKG_TARGET_TRIPLET" || true
./vcpkg remove onnx --triplet "$VCPKG_TARGET_TRIPLET" || true
echo -e "${GREEN}✓ 已移除旧包${NC}"

echo ""
echo -e "${BLUE}步骤 2: 使用 overlay-ports 重新安装 onnx（应用 patch）...${NC}"
./vcpkg install onnx --triplet "$VCPKG_TARGET_TRIPLET" --overlay-ports="$PROJECT_DIR/ports"
if [ $? -ne 0 ]; then
    echo -e "${RED}错误: onnx 安装失败${NC}"
    exit 1
fi
echo -e "${GREEN}✓ onnx 安装成功（已应用 patch）${NC}"

echo ""
echo -e "${BLUE}步骤 3: 重新安装 onnxruntime...${NC}"
./vcpkg install onnxruntime --triplet "$VCPKG_TARGET_TRIPLET"
if [ $? -ne 0 ]; then
    echo -e "${RED}错误: onnxruntime 安装失败${NC}"
    exit 1
fi
echo -e "${GREEN}✓ onnxruntime 安装成功${NC}"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✓ Patch 应用成功！${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}下一步:${NC}"
echo -e "${BLUE}  1. 重新编译项目:${NC}"
echo -e "${GREEN}    cd build/macos-arm64-Release${NC}"
echo -e "${GREEN}    rm -rf *${NC}"
echo -e "${GREEN}    cmake ../..${NC}"
echo -e "${GREEN}    make${NC}"
echo ""
echo -e "${BLUE}  2. 运行程序验证修复:${NC}"
echo -e "${GREEN}    ./bin/detector_service${NC}"
echo ""
