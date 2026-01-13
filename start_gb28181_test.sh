#!/bin/bash

# GB28181 测试环境快速启动脚本

set -e

echo "=========================================="
echo "启动 GB28181 测试环境"
echo "=========================================="

# 检查配置文件是否存在
if [ ! -f "gb28181-test-data/srs/srs.conf" ]; then
    echo "❌ 配置文件不存在: gb28181-test-data/srs/srs.conf"
    echo "请确保配置文件已创建"
    exit 1
fi

# 检查 Docker 是否运行
if ! docker info > /dev/null 2>&1; then
    echo "❌ Docker 未运行，请先启动 Docker"
    exit 1
fi

# 停止已存在的容器
echo "停止已存在的容器..."
docker-compose -f docker-compose-gb28181-test.yml down 2>/dev/null || true

# 启动容器
echo "启动 SRS GB28181 服务器..."
docker-compose -f docker-compose-gb28181-test.yml up -d

# 等待服务启动
echo "等待服务启动..."
sleep 3

# 检查容器状态
if docker ps | grep -q gb28181-srs; then
    echo "✓ SRS 容器已启动"
else
    echo "❌ SRS 容器启动失败"
    echo "查看日志: docker logs gb28181-srs"
    exit 1
fi

# 检查 API 是否可访问
echo "检查 API 连接..."
for i in {1..10}; do
    if curl -s http://localhost:1986/api/v1/summaries > /dev/null 2>&1; then
        echo "✓ SRS API 可访问"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "⚠ SRS API 暂时不可访问，请稍后重试"
    else
        sleep 1
    fi
done

echo ""
echo "=========================================="
echo "测试环境已启动"
echo "=========================================="
echo "SRS API: http://localhost:1986"
echo "测试页面: http://localhost:8081"
echo "SIP 端口: 5062"
echo ""
echo "查看日志: docker logs -f gb28181-srs"
echo "运行测试: ./test_gb28181_registration.sh"
echo "停止服务: docker-compose -f docker-compose-gb28181-test.yml down"
echo ""

