#!/bin/bash

# Docker 构建脚本 - ARM64 Ubuntu 20.04
# 用于构建 detector_service 项目的 ARM64 版本

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默认配置
IMAGE_NAME="detector-service"
IMAGE_TAG="arm64-ubuntu20.04"
DOCKERFILE="Dockerfile.arm64"
BUILD_TYPE="Release"
NO_CACHE=false
PUSH=false
REGISTRY=""
USE_BUILDX_OVERRIDE=""

# 显示帮助信息
show_help() {
    cat << EOF
Docker 构建脚本 - ARM64 Ubuntu 20.04

用法: $0 [选项]

选项:
    -n, --name NAME          镜像名称 [默认: detector-service]
    -t, --tag TAG            镜像标签 [默认: arm64-ubuntu20.04]
    -f, --file FILE          Dockerfile 路径 [默认: Dockerfile.arm64]
    --no-cache               不使用缓存构建
    --push                   构建后推送到仓库
    -r, --registry REGISTRY  镜像仓库地址（用于推送）
    --no-buildx              不使用 Buildx，使用标准 Docker 构建（避免长时间构建时的连接问题）
    -h, --help               显示此帮助信息

示例:
    $0                                    # 基本构建
    $0 -t v1.0.0                          # 指定标签
    $0 --no-cache                         # 不使用缓存
    $0 --push -r registry.example.com    # 构建并推送
EOF
}

# 解析命令行参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -n|--name)
                IMAGE_NAME="$2"
                shift 2
                ;;
            -t|--tag)
                IMAGE_TAG="$2"
                shift 2
                ;;
            -f|--file)
                DOCKERFILE="$2"
                shift 2
                ;;
            --no-cache)
                NO_CACHE=true
                shift
                ;;
            --push)
                PUSH=true
                shift
                ;;
            -r|--registry)
                REGISTRY="$2"
                shift 2
                ;;
            --no-buildx)
                USE_BUILDX_OVERRIDE=false
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
        esac
    done
}

# 检查 Docker 是否安装
check_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        echo -e "${RED}错误: 未找到 Docker${NC}"
        echo -e "${YELLOW}请安装 Docker: https://docs.docker.com/get-docker/${NC}"
        exit 1
    fi
    
    DOCKER_VERSION=$(docker --version)
    echo -e "${GREEN}找到 Docker: $DOCKER_VERSION${NC}"
}

# 检查 Docker Buildx（用于多架构构建）
check_buildx() {
    if ! docker buildx version >/dev/null 2>&1; then
        echo -e "${YELLOW}警告: Docker Buildx 未找到，将使用标准构建${NC}"
        echo -e "${BLUE}建议安装 Buildx 以支持多架构构建${NC}"
        USE_BUILDX=false
    else
        USE_BUILDX=true
        echo -e "${GREEN}找到 Docker Buildx${NC}"
        
        # 检查是否已创建 builder 实例
        if ! docker buildx ls | grep -q "multiarch"; then
            echo -e "${BLUE}创建多架构构建器...${NC}"
            docker buildx create --name multiarch --use --bootstrap || true
        fi
    fi
}

# 构建镜像
build_image() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}开始构建 Docker 镜像${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${BLUE}镜像名称: ${IMAGE_NAME}${NC}"
    echo -e "${BLUE}镜像标签: ${IMAGE_TAG}${NC}"
    echo -e "${BLUE}Dockerfile: ${DOCKERFILE}${NC}"
    echo ""
    
    # 检查 Dockerfile 是否存在
    if [ ! -f "$DOCKERFILE" ]; then
        echo -e "${RED}错误: Dockerfile 不存在: ${DOCKERFILE}${NC}"
        exit 1
    fi
    
    # 构建参数
    BUILD_ARGS=(
        --platform linux/arm64
        -f "$DOCKERFILE"
        -t "${IMAGE_NAME}:${IMAGE_TAG}"
        -t "${IMAGE_NAME}:latest"
    )
    
    # 添加构建参数
    BUILD_ARGS+=(
        --build-arg VCPKG_VERSION=2024.01.12
    )
    
    # 不使用缓存
    if [ "$NO_CACHE" = true ]; then
        BUILD_ARGS+=(--no-cache)
        echo -e "${YELLOW}不使用缓存构建${NC}"
    fi
    
    # 使用 Buildx 或标准构建
    # 如果用户明确指定不使用 Buildx，则覆盖检测结果
    if [ "$USE_BUILDX_OVERRIDE" = "false" ]; then
        USE_BUILDX=false
        echo -e "${YELLOW}已禁用 Buildx，使用标准 Docker 构建${NC}"
    fi
    
    if [ "$USE_BUILDX" = true ]; then
        echo -e "${BLUE}使用 Docker Buildx 构建...${NC}"
        docker buildx build "${BUILD_ARGS[@]}" \
            --load \
            --progress=plain \
            .
    else
        echo -e "${BLUE}使用标准 Docker 构建...${NC}"
        docker build "${BUILD_ARGS[@]}" .
    fi
    
    echo -e "${GREEN}构建完成！${NC}"
}

# 推送镜像
push_image() {
    if [ "$PUSH" = true ]; then
        echo -e "${BLUE}推送镜像到仓库...${NC}"
        
        if [ -n "$REGISTRY" ]; then
            FULL_IMAGE_NAME="${REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}"
            docker tag "${IMAGE_NAME}:${IMAGE_TAG}" "$FULL_IMAGE_NAME"
            docker push "$FULL_IMAGE_NAME"
            echo -e "${GREEN}镜像已推送到: ${FULL_IMAGE_NAME}${NC}"
        else
            echo -e "${YELLOW}警告: 未指定仓库地址，跳过推送${NC}"
            echo -e "${BLUE}使用 -r 选项指定仓库地址${NC}"
        fi
    fi
}

# 显示镜像信息
show_image_info() {
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}构建成功！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${BLUE}镜像名称: ${IMAGE_NAME}:${IMAGE_TAG}${NC}"
    echo ""
    echo -e "${BLUE}运行容器:${NC}"
    echo -e "${GREEN}  docker run -it --rm ${IMAGE_NAME}:${IMAGE_TAG}${NC}"
    echo ""
    echo -e "${BLUE}查看镜像信息:${NC}"
    echo -e "${GREEN}  docker inspect ${IMAGE_NAME}:${IMAGE_TAG}${NC}"
    echo ""
}

# 主函数
main() {
    parse_args "$@"
    
    check_docker
    check_buildx
    build_image
    push_image
    show_image_info
}

# 运行主函数
main "$@"

