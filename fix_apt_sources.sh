#!/bin/bash
# 修复 apt 源配置，使用国内镜像源

set -e

echo "=========================================="
echo "修复 apt 源配置脚本"
echo "=========================================="
echo ""

# 检查是否已配置镜像源
if grep -q "mirrors.tuna.tsinghua.edu.cn" /etc/apt/sources.list.d/ubuntu.sources 2>/dev/null; then
    echo "✓ 已检测到清华大学镜像源配置"
else
    echo "正在备份原始源文件..."
    sudo cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources.backup
    
    echo "正在配置使用清华大学镜像源..."
    sudo sed -i 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources
    sudo sed -i 's|http://security.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources
    echo "✓ 镜像源配置完成"
fi

echo ""
echo "正在更新软件包列表..."
if sudo apt-get update; then
    echo "✓ 软件包列表更新成功"
else
    echo "✗ 软件包列表更新失败，请检查网络连接"
    exit 1
fi

echo ""
echo "正在验证 ARM64 交叉编译工具链是否可用..."
if apt-cache search gcc-aarch64-linux-gnu | grep -q "gcc-aarch64-linux-gnu"; then
    echo "✓ 找到 ARM64 交叉编译工具链"
    echo ""
    echo "现在可以安装 ARM64 交叉编译工具链了："
    echo "  sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
else
    echo "✗ 未找到 ARM64 交叉编译工具链"
    echo ""
    echo "可能的原因："
    echo "  1. 软件包列表未完全更新，请重试: sudo apt-get update"
    echo "  2. universe 仓库未启用"
    echo ""
    echo "检查 universe 仓库配置："
    grep -A 1 "Components:" /etc/apt/sources.list.d/ubuntu.sources | head -2
fi

