# Docker 构建指南 - ARM64 Ubuntu 20.04

本文档说明如何使用 Docker 构建 ARM64 Ubuntu 20.04 环境下的 detector_service 项目。

## 前置要求

1. **Docker** (版本 20.10+)
   ```bash
   # 检查 Docker 版本
   docker --version
   ```

2. **Docker Buildx** (推荐，用于多架构构建)
   ```bash
   # 检查 Buildx 是否可用
   docker buildx version
   
   # 如果未安装，Docker Desktop 通常已包含
   # Linux 上可能需要单独安装
   ```

3. **QEMU** (如果在非 ARM 机器上构建)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install qemu-user-static binfmt-support
   
   # 注册 QEMU 解释器
   docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
   ```

## 快速开始

### 方法 1: 使用构建脚本（推荐）

```bash
# 基本构建
./docker-build-arm64.sh

# 指定标签
./docker-build-arm64.sh -t v1.0.0

# 不使用缓存构建
./docker-build-arm64.sh --no-cache

# 构建并推送到仓库
./docker-build-arm64.sh --push -r registry.example.com
```

### 方法 2: 使用 Docker Compose

```bash
# 构建并启动服务
docker-compose -f docker-compose.arm64.yml up --build

# 后台运行
docker-compose -f docker-compose.arm64.yml up -d --build

# 停止服务
docker-compose -f docker-compose.arm64.yml down
```

### 方法 3: 直接使用 Docker 命令

```bash
# 构建镜像
docker build \
  --platform linux/arm64 \
  -f Dockerfile.arm64 \
  -t detector-service:arm64-ubuntu20.04 \
  .

# 运行容器
docker run -it --rm \
  --platform linux/arm64 \
  -p 8080:8080 \
  detector-service:arm64-ubuntu20.04
```

## 构建说明

### 多阶段构建

Dockerfile 使用多阶段构建：

1. **builder 阶段**: 包含所有构建工具和依赖
   - Ubuntu 20.04 ARM64
   - CMake, Ninja, GCC
   - vcpkg 包管理器
   - 编译项目

2. **runtime 阶段**: 仅包含运行时依赖
   - 最小化的 Ubuntu 20.04 ARM64
   - 运行时库
   - 编译好的可执行文件

### 构建参数

可以通过 `--build-arg` 传递参数：

```bash
docker build \
  --build-arg VCPKG_VERSION=2024.01.12 \
  -f Dockerfile.arm64 \
  -t detector-service:arm64-ubuntu20.04 \
  .
```

### vcpkg 依赖

项目依赖通过 `vcpkg.json` 自动安装，包括：
- OpenCV4
- FFmpeg
- ONNX Runtime
- Crow (HTTP 框架)
- SQLite3
- Mosquitto (MQTT)
- 其他依赖

**注意**: 首次构建时，vcpkg 需要下载和编译大量依赖，可能需要数小时。

## 在非 ARM 机器上构建

如果在 x86_64 机器上构建 ARM64 镜像，需要：

1. **启用 QEMU 模拟器**:
   ```bash
   docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
   ```

2. **使用 Buildx**:
   ```bash
   # 创建多架构构建器
   docker buildx create --name multiarch --use --bootstrap
   
   # 构建
   docker buildx build \
     --platform linux/arm64 \
     -f Dockerfile.arm64 \
     -t detector-service:arm64-ubuntu20.04 \
     --load \
     .
   ```

**注意**: 在 x86_64 上模拟 ARM64 构建会很慢，建议在 ARM64 机器上直接构建。

## 优化构建

### 使用构建缓存

Docker 会自动缓存构建层，但可以通过以下方式优化：

1. **使用 .dockerignore**: 排除不必要的文件
2. **分层构建**: 将依赖安装和代码编译分开
3. **使用 BuildKit**: 启用并行构建

```bash
# 启用 BuildKit
export DOCKER_BUILDKIT=1
export COMPOSE_DOCKER_CLI_BUILD=1
```

### 加速 vcpkg 安装

1. **使用预编译的 vcpkg 包** (如果可用)
2. **使用本地 vcpkg 缓存**:
   ```bash
   # 挂载本地 vcpkg 目录
   docker build \
     --build-arg VCPKG_ROOT=/opt/vcpkg \
     -v /path/to/local/vcpkg:/opt/vcpkg \
     ...
   ```

## 故障排除

### 1. 构建失败：找不到 vcpkg

**问题**: vcpkg 下载失败或版本不匹配

**解决**:
```bash
# 检查网络连接
# 使用代理（如果需要）
docker build --build-arg HTTP_PROXY=http://proxy:port ...
```

### 2. 内存不足

**问题**: 编译大型依赖（如 OpenCV）时内存不足

**解决**:
- 增加 Docker 内存限制
- 减少并行编译任务数
- 使用预编译的依赖包

### 3. 架构不匹配

**问题**: 在错误的架构上运行

**解决**:
```bash
# 检查镜像架构
docker inspect detector-service:arm64-ubuntu20.04 | grep Architecture

# 确保使用 --platform 参数
docker run --platform linux/arm64 ...
```

### 4. 库文件缺失

**问题**: 运行时找不到共享库

**解决**:
```bash
# 检查库路径
docker run --rm detector-service:arm64-ubuntu20.04 ldd /usr/local/bin/detector_service

# 确保 LD_LIBRARY_PATH 正确设置
```

## 验证构建

```bash
# 检查镜像
docker images detector-service:arm64-ubuntu20.04

# 检查镜像架构
docker inspect detector-service:arm64-ubuntu20.04 | grep Architecture

# 运行测试
docker run --rm detector-service:arm64-ubuntu20.04 detector_service --version
```

## 相关文件

- `Dockerfile.arm64`: ARM64 Ubuntu 20.04 构建文件
- `docker-build-arm64.sh`: 构建脚本
- `docker-compose.arm64.yml`: Docker Compose 配置
- `.dockerignore`: Docker 忽略文件

## 更多信息

- [Docker 多架构构建文档](https://docs.docker.com/build/building/multi-platform/)
- [vcpkg 文档](https://vcpkg.io/)
- [Ubuntu 20.04 官方镜像](https://hub.docker.com/_/ubuntu)

