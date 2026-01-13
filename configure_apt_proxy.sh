#!/bin/bash
# 配置 apt 使用代理

set -e

echo "=========================================="
echo "配置 apt 使用代理"
echo "=========================================="
echo ""

# 检查环境变量中的代理设置
if [ -n "$HTTP_PROXY" ]; then
    PROXY_URL="$HTTP_PROXY"
    echo "检测到 HTTP_PROXY: $PROXY_URL"
elif [ -n "$HTTPS_PROXY" ]; then
    PROXY_URL="$HTTPS_PROXY"
    echo "检测到 HTTPS_PROXY: $PROXY_URL"
else
    echo "未检测到代理环境变量"
    echo "请设置 HTTP_PROXY 或 HTTPS_PROXY 环境变量"
    exit 1
fi

# 创建 apt 代理配置文件
echo "正在配置 apt 使用代理..."
sudo tee /etc/apt/apt.conf.d/95proxies > /dev/null <<EOF
Acquire::http::Proxy "$PROXY_URL";
Acquire::https::Proxy "$PROXY_URL";
EOF

echo "✓ apt 代理配置完成"
echo ""
echo "现在可以尝试更新软件包列表："
echo "  sudo apt-get update"

