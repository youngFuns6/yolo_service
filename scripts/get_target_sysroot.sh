#!/bin/bash
# 从目标系统获取 sysroot 的辅助脚本
# 在目标系统上运行此脚本以创建 sysroot 包

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}目标系统 Sysroot 提取工具${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查是否在目标系统上
ARCH=$(uname -m)
if [ "$ARCH" != "aarch64" ]; then
    echo -e "${YELLOW}警告: 当前系统架构是 $ARCH，不是 aarch64${NC}"
    echo -e "${YELLOW}如果这是交叉编译环境，请确保在目标系统上运行此脚本${NC}"
    read -p "是否继续? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# 检查 GLIBC 版本
echo -e "${BLUE}检查系统信息...${NC}"
GLIBC_VERSION=$(ldd --version | head -1)
echo -e "${GREEN}GLIBC 版本: $GLIBC_VERSION${NC}"
echo -e "${GREEN}系统架构: $ARCH${NC}"
echo ""

# 设置输出目录
OUTPUT_DIR="${1:-./target-sysroot}"
OUTPUT_TAR="${OUTPUT_DIR}.tar.gz"

echo -e "${BLUE}创建 sysroot 目录: $OUTPUT_DIR${NC}"
mkdir -p "$OUTPUT_DIR"

# 需要复制的目录
DIRS_TO_COPY=(
    "/lib"
    "/usr/lib"
    "/usr/include"
    "/usr/aarch64-linux-gnu"
    "/usr/local/lib"
)

echo -e "${BLUE}复制系统库和头文件...${NC}"
for dir in "${DIRS_TO_COPY[@]}"; do
    if [ -d "$dir" ]; then
        echo -e "  复制: $dir"
        # 创建目标目录结构
        TARGET_DIR="$OUTPUT_DIR$dir"
        mkdir -p "$(dirname "$TARGET_DIR")"
        # 复制目录内容
        cp -r "$dir" "$TARGET_DIR" 2>/dev/null || {
            echo -e "${YELLOW}  警告: 无法复制 $dir（某些文件可能需要 root 权限）${NC}"
        }
    else
        echo -e "${YELLOW}  跳过: $dir (不存在)${NC}"
    fi
done

# 复制重要的系统文件
echo -e "${BLUE}复制系统配置文件...${NC}"
if [ -f "/etc/ld.so.conf" ]; then
    mkdir -p "$OUTPUT_DIR/etc"
    cp /etc/ld.so.conf "$OUTPUT_DIR/etc/" 2>/dev/null || true
fi

if [ -d "/etc/ld.so.conf.d" ]; then
    mkdir -p "$OUTPUT_DIR/etc/ld.so.conf.d"
    cp -r /etc/ld.so.conf.d/* "$OUTPUT_DIR/etc/ld.so.conf.d/" 2>/dev/null || true
fi

# 创建信息文件
INFO_FILE="$OUTPUT_DIR/SYSROOT_INFO.txt"
cat > "$INFO_FILE" << EOF
Sysroot 信息
============
创建时间: $(date)
系统架构: $ARCH
GLIBC 版本: $GLIBC_VERSION
内核版本: $(uname -r)
发行版: $(lsb_release -d 2>/dev/null | cut -f2 || echo "未知")

使用说明:
1. 将此 sysroot 传输到编译机器
2. 使用以下命令编译:
   ./build.sh -a arm64 --sysroot $(pwd)/$OUTPUT_DIR
EOF

echo -e "${GREEN}Sysroot 信息已保存到: $INFO_FILE${NC}"

# 打包
echo -e "${BLUE}打包 sysroot...${NC}"
tar czf "$OUTPUT_TAR" -C "$(dirname "$OUTPUT_DIR")" "$(basename "$OUTPUT_DIR")"

# 计算大小
SIZE=$(du -sh "$OUTPUT_DIR" | cut -f1)
TAR_SIZE=$(du -sh "$OUTPUT_TAR" | cut -f1)

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Sysroot 创建完成!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "目录: $OUTPUT_DIR (大小: $SIZE)"
echo -e "压缩包: $OUTPUT_TAR (大小: $TAR_SIZE)"
echo ""
echo -e "${BLUE}下一步:${NC}"
echo -e "1. 将 $OUTPUT_TAR 传输到编译机器"
echo -e "2. 解压: tar xzf $OUTPUT_TAR"
echo -e "3. 编译: ./build.sh -a arm64 --sysroot $(pwd)/$OUTPUT_DIR"
echo ""

