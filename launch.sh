#!/bin/bash

# 视觉分析服务启动脚本
# 过滤 ONNX schema 注册警告日志

set -e

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 默认配置
BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

# 查找可执行文件
# 优先查找指定目录，然后搜索所有构建目录
EXECUTABLE=""

# 1. 先尝试简单的 build/bin/ 路径
if [ -f "${BUILD_DIR}/bin/detector_service" ]; then
    EXECUTABLE="${BUILD_DIR}/bin/detector_service"
elif [ -f "${BUILD_DIR}/bin/detector_service.exe" ]; then
    EXECUTABLE="${BUILD_DIR}/bin/detector_service.exe"
# 2. 搜索所有构建子目录
elif [ -d "${BUILD_DIR}" ]; then
    # 查找所有可能的构建目录中的可执行文件
    FOUND=$(find "${BUILD_DIR}" -type f \( -name "detector_service" -o -name "detector_service.exe" \) 2>/dev/null | head -1)
    if [ -n "$FOUND" ] && [ -f "$FOUND" ]; then
        EXECUTABLE="$FOUND"
    fi
fi

# 如果还是找不到，报错
if [ -z "$EXECUTABLE" ] || [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}错误: 找不到可执行文件${NC}"
    echo -e "${YELLOW}请先运行构建脚本: ./build.sh${NC}"
    echo -e "${BLUE}或者设置 BUILD_DIR 环境变量指定构建目录${NC}"
    exit 1
fi

# 检查模型文件
# 默认模型路径是 yolov11n.onnx，如果不存在，尝试在 models 目录查找
MODEL_FILE="yolov11n.onnx"
MODEL_IN_MODELS="models/yolov11n.onnx"

if [ ! -f "$MODEL_FILE" ]; then
    if [ -f "$MODEL_IN_MODELS" ]; then
        echo -e "${YELLOW}提示: 在 models 目录找到模型文件，创建符号链接...${NC}"
        # 尝试创建符号链接
        if ln -sf "$MODEL_IN_MODELS" "$MODEL_FILE" 2>/dev/null; then
            echo -e "${GREEN}✓ 符号链接创建成功${NC}"
        else
            # 如果符号链接失败，尝试复制文件（作为备选方案）
            echo -e "${YELLOW}符号链接创建失败，尝试复制文件...${NC}"
            if cp "$MODEL_IN_MODELS" "$MODEL_FILE" 2>/dev/null; then
                echo -e "${GREEN}✓ 模型文件已复制到项目根目录${NC}"
            else
                echo -e "${RED}错误: 无法创建符号链接或复制模型文件${NC}"
                echo -e "${YELLOW}请手动将 $MODEL_IN_MODELS 复制或链接到 $MODEL_FILE${NC}"
                exit 1
            fi
        fi
    else
        echo -e "${RED}错误: 未找到模型文件${NC}"
        echo -e "${YELLOW}请确保模型文件存在于以下位置之一:${NC}"
        echo -e "${BLUE}  - $MODEL_FILE${NC}"
        echo -e "${BLUE}  - $MODEL_IN_MODELS${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}启动视觉分析服务${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${BLUE}可执行文件: ${EXECUTABLE}${NC}"
echo -e "${BLUE}过滤日志: ONNX schema 注册警告${NC}"
echo ""

# 运行程序并过滤日志
# 使用 sed 精确过滤 ONNX schema 注册警告和空行
# 只删除以 "Schema error: Trying to register schema" 开头且包含 "but it is already registered" 的行
# 同时删除空行，保留所有正常日志
# 使用 stdbuf 禁用缓冲，确保日志实时输出
set +e  # 临时禁用 set -e，以便正确处理管道
stdbuf -oL -eL "$EXECUTABLE" "$@" 2>&1 | stdbuf -oL -eL sed -e '/^Schema error: Trying to register schema with name.*but it is already registered/d' -e '/^$/d'
EXIT_CODE=${PIPESTATUS[0]}
set -e  # 重新启用 set -e

# 如果程序正常退出，返回其退出码
exit $EXIT_CODE
