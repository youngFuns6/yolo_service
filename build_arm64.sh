#!/bin/bash
# Linux ARM64 交叉编译完整流程脚本

set -e

echo "=========================================="
echo "Linux ARM64 交叉编译脚本"
echo "=========================================="
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 检查工具链
check_toolchain() {
    if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
        echo -e "${GREEN}✓ ARM64 交叉编译工具链已安装${NC}"
        aarch64-linux-gnu-gcc --version | head -1
        return 0
    else
        echo -e "${YELLOW}✗ ARM64 交叉编译工具链未安装${NC}"
        return 1
    fi
}

# 安装工具链
install_toolchain() {
    echo -e "${BLUE}正在配置 apt 代理...${NC}"
    
    # 检查代理环境变量
    if [ -n "$HTTP_PROXY" ]; then
        PROXY_URL="$HTTP_PROXY"
    elif [ -n "$HTTPS_PROXY" ]; then
        PROXY_URL="$HTTPS_PROXY"
    else
        echo -e "${YELLOW}警告: 未检测到代理环境变量${NC}"
        echo -e "${YELLOW}如果网络连接有问题，请先设置 HTTP_PROXY 或 HTTPS_PROXY${NC}"
    fi
    
    if [ -n "$PROXY_URL" ]; then
        echo "配置 apt 使用代理: $PROXY_URL"
        sudo tee /etc/apt/apt.conf.d/95proxies > /dev/null <<EOF
Acquire::http::Proxy "$PROXY_URL";
Acquire::https::Proxy "$PROXY_URL";
EOF
        echo -e "${GREEN}✓ apt 代理配置完成${NC}"
    fi
    
    echo -e "${BLUE}正在更新软件包列表...${NC}"
    sudo apt-get update
    
    echo -e "${BLUE}正在安装 ARM64 交叉编译工具链...${NC}"
    sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
    
    if check_toolchain; then
        echo -e "${GREEN}✓ 工具链安装成功${NC}"
    else
        echo -e "${RED}✗ 工具链安装失败${NC}"
        exit 1
    fi
}

# 执行构建
build_project() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}开始构建 Linux ARM64 版本${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    cd "$(dirname "$0")"
    ./build.sh -a arm64
}

# 主流程
main() {
    if ! check_toolchain; then
        echo ""
        echo -e "${YELLOW}需要安装 ARM64 交叉编译工具链${NC}"
        read -p "是否现在安装? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            install_toolchain
        else
            echo -e "${RED}取消构建：需要先安装 ARM64 交叉编译工具链${NC}"
            echo ""
            echo "手动安装命令："
            echo "  sudo apt-get update"
            echo "  sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
            exit 1
        fi
    fi
    
    build_project
}

main "$@"

