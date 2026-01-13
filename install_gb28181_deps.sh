#!/bin/bash

# GB28181 SIP依赖安装脚本

set -e

echo "=========================================="
echo "GB28181 SIP依赖安装脚本"
echo "=========================================="
echo ""

# 检测操作系统
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    OS_VERSION=$VERSION_ID
else
    echo "无法检测操作系统类型"
    exit 1
fi

echo "检测到操作系统: $OS $OS_VERSION"
echo ""

# 根据操作系统安装依赖
case $OS in
    ubuntu|debian)
        echo "使用 apt-get 安装依赖..."
        sudo apt-get update
        sudo apt-get install -y libexosip2-dev libosip2-dev pkg-config
        ;;
    centos|rhel|fedora)
        if [ "$OS" = "fedora" ]; then
            echo "使用 dnf 安装依赖..."
            sudo dnf install -y libeXosip2-devel libosip2-devel pkg-config
        else
            echo "使用 yum 安装依赖..."
            sudo yum install -y libeXosip2-devel libosip2-devel pkg-config
        fi
        ;;
    arch|manjaro)
        echo "使用 pacman 安装依赖..."
        sudo pacman -S --noconfirm libexosip2 libosip2 pkg-config
        ;;
    *)
        echo "不支持的操作系统: $OS"
        echo "请手动安装以下包："
        echo "  - libexosip2-dev (或 libeXosip2-devel)"
        echo "  - libosip2-dev (或 libosip2-devel)"
        echo "  - pkg-config"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "验证安装..."
echo "=========================================="

# 验证 eXosip2
if pkg-config --exists eXosip2; then
    EXOSIP2_VERSION=$(pkg-config --modversion eXosip2)
    echo "✓ eXosip2 已安装: $EXOSIP2_VERSION"
else
    echo "✗ eXosip2 未找到"
    echo "  请检查安装是否成功"
    exit 1
fi

# 验证 osip2
if pkg-config --exists osip2; then
    OSIP2_VERSION=$(pkg-config --modversion osip2)
    echo "✓ osip2 已安装: $OSIP2_VERSION"
else
    echo "✗ osip2 未找到"
    echo "  请检查安装是否成功"
    exit 1
fi

echo ""
echo "=========================================="
echo "✓ 所有依赖安装成功！"
echo "=========================================="
echo ""
echo "现在可以构建项目了："
echo "  ./build.sh"
echo ""

