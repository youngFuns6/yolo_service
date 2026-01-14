#!/bin/bash

# GB28181 Docker 镜像修复脚本
# 用于解决镜像拉取失败的问题

set -e

echo "=========================================="
echo "GB28181 Docker 镜像修复工具"
echo "=========================================="
echo ""

# 检查 Docker 是否运行
if ! docker info > /dev/null 2>&1; then
    echo "❌ Docker 未运行，请先启动 Docker"
    exit 1
fi

IMAGE_NAME="7040210/gb28181:latest"
CONTAINER_NAME="gb28181-srs"

# 检查镜像是否已存在
if docker images | grep -q "7040210/gb28181"; then
    echo "✓ 镜像已存在于本地: $IMAGE_NAME"
    echo "可以直接运行: ./start_gb28181_test.sh"
    exit 0
fi

echo "尝试从不同源拉取镜像..."
echo ""

# 方案1: 直接从 Docker Hub 拉取（不使用镜像源）
echo "方案 1: 从 Docker Hub 直接拉取..."
if docker pull "$IMAGE_NAME" 2>/dev/null; then
    echo "✓ 成功从 Docker Hub 拉取镜像"
    exit 0
fi

# 方案2: 尝试从阿里云镜像源拉取
echo ""
echo "方案 2: 从阿里云镜像源拉取..."
ALIYUN_IMAGE="registry.cn-hangzhou.aliyuncs.com/$IMAGE_NAME"
if docker pull "$ALIYUN_IMAGE" 2>/dev/null; then
    echo "✓ 成功从阿里云拉取镜像"
    echo "正在标记镜像..."
    docker tag "$ALIYUN_IMAGE" "$IMAGE_NAME"
    echo "✓ 镜像已标记为 $IMAGE_NAME"
    exit 0
fi

# 方案3: 使用官方 SRS 镜像（需要确认是否支持 GB28181）
echo ""
echo "方案 3: 使用官方 SRS 镜像（可能不支持 GB28181）..."
echo "⚠ 警告: 官方 SRS 镜像可能不包含 GB28181 支持"
read -p "是否尝试使用 ossrs/srs:4 镜像？(y/N): " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if docker pull ossrs/srs:4 2>/dev/null; then
        echo "✓ 成功拉取 ossrs/srs:4 镜像"
        echo ""
        echo "⚠ 注意: 需要修改 docker-compose-gb28181-test.yml"
        echo "将 image: $IMAGE_NAME 改为 image: ossrs/srs:4"
        echo ""
        read -p "是否自动修改配置文件？(y/N): " -n 1 -r
        echo ""
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            # 备份原文件
            cp docker-compose-gb28181-test.yml docker-compose-gb28181-test.yml.bak
            # 修改镜像
            if [[ "$OSTYPE" == "darwin"* ]]; then
                # macOS
                sed -i '' "s|image: 7040210/gb28181:latest|image: ossrs/srs:4|g" docker-compose-gb28181-test.yml
            else
                # Linux
                sed -i "s|image: 7040210/gb28181:latest|image: ossrs/srs:4|g" docker-compose-gb28181-test.yml
            fi
            echo "✓ 配置文件已修改"
            echo "备份文件: docker-compose-gb28181-test.yml.bak"
        fi
        exit 0
    fi
fi

# 所有方案都失败
echo ""
echo "❌ 所有镜像拉取方案都失败了"
echo ""
echo "手动解决方案："
echo ""
echo "1. 检查网络连接和防火墙设置"
echo ""
echo "2. 配置 Docker 镜像加速器（推荐）:"
echo "   编辑 Docker Desktop -> Settings -> Docker Engine"
echo "   添加镜像源配置，例如："
echo '   {'
echo '     "registry-mirrors": ['
echo '       "https://docker.mirrors.ustc.edu.cn",'
echo '       "https://hub-mirror.c.163.com"'
echo '     ]'
echo '   }'
echo ""
echo "3. 手动拉取镜像:"
echo "   docker pull $IMAGE_NAME"
echo ""
echo "4. 或从其他镜像源拉取:"
echo "   docker pull registry.cn-hangzhou.aliyuncs.com/$IMAGE_NAME"
echo "   docker tag registry.cn-hangzhou.aliyuncs.com/$IMAGE_NAME $IMAGE_NAME"
echo ""
echo "5. 使用备选镜像（修改 docker-compose-gb28181-test.yml）:"
echo "   将 image: $IMAGE_NAME"
echo "   改为: image: ossrs/srs:4"
echo "   （注意：官方镜像可能不支持 GB28181）"
echo ""
echo "6. 如果使用 Windows，可以尝试:"
echo "   在 PowerShell 中运行:"
echo "   docker pull $IMAGE_NAME"
echo ""

exit 1

