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

# 检查镜像是否存在
echo "检查 Docker 镜像..."
if ! docker images | grep -q "7040210/gb28181"; then
    echo "⚠ 镜像不存在于本地，尝试拉取..."
    if ! docker pull 7040210/gb28181:latest 2>/dev/null; then
        echo ""
        echo "❌ 无法拉取镜像 7040210/gb28181:latest"
        echo ""
        echo "正在运行镜像修复脚本..."
        if [ -f "./fix_gb28181_image.sh" ]; then
            ./fix_gb28181_image.sh
            if [ $? -ne 0 ]; then
                echo ""
                echo "修复脚本执行失败，请手动解决镜像问题后重试"
                exit 1
            fi
        else
            echo "修复脚本不存在，请手动解决："
            echo "1. 运行修复脚本: ./fix_gb28181_image.sh"
            echo "2. 或手动拉取镜像: docker pull 7040210/gb28181:latest"
            echo "3. 或修改 docker-compose-gb28181-test.yml 使用备选镜像"
            exit 1
        fi
    else
        echo "✓ 镜像拉取成功"
    fi
else
    echo "✓ 镜像已存在于本地"
fi

# 启动容器
echo "启动 SRS GB28181 服务器..."
if ! docker-compose -f docker-compose-gb28181-test.yml up -d; then
    echo ""
    echo "❌ 容器启动失败"
    echo ""
    echo "解决方案："
    echo "1. 检查镜像是否存在: docker images | grep gb28181"
    echo "2. 尝试使用备选镜像:"
    echo "   编辑 docker-compose-gb28181-test.yml"
    echo "   将 image: 7040210/gb28181:latest"
    echo "   改为: image: ossrs/srs:4"
    echo "3. 或者从其他源拉取镜像:"
    echo "   docker pull registry.cn-hangzhou.aliyuncs.com/7040210/gb28181:latest"
    echo ""
    exit 1
fi

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

