# BM1684 Docker 编译指南

本文档说明如何在 Docker 环境中编译 BM1684 版本。

## 前置要求

1. **Docker** (版本 20.10+)
2. **SophonSDK Docker 镜像** (如 `sophgo/sophonsdk3`)

## 快速开始

### 方法 1: 使用官方 Docker 镜像（推荐）

**重要说明**：使用挂载方式，**不需要**把项目复制到容器里。项目代码保留在宿主机，容器中通过挂载访问。

```bash
# 在项目根目录下执行
# -v $(pwd):/workspace 将当前目录挂载到容器的 /workspace
# 这样容器中可以直接访问和修改宿主机上的项目文件
docker run -it --rm \
    -v $(pwd):/workspace \
    -w /workspace \
    -e LOCAL_USER_ID=$(id -u) \
    sophgo/sophonsdk3:ubuntu18.04-py37-dev-22.06 \
    bash

# 在容器中编译（编译后的文件会保存在宿主机的 build/ 目录中）
./build-bm1684-arm64.sh
```

**优点**：
- ✅ 项目代码保留在宿主机，方便编辑和版本控制
- ✅ 编译后的文件直接保存在宿主机，无需从容器中复制
- ✅ 容器删除后，所有文件都在宿主机上
- ✅ 可以使用宿主机的 IDE 和工具编辑代码
- ✅ 支持热重载，修改代码后可直接重新编译

### 方法 2: 使用 SDK 提供的 Docker 脚本

如果 SDK 中包含 `docker_run_sophonsdk.sh`：

```bash
# 设置 SDK 路径
export REL_TOP=/path/to/sophonsdk_v3.0.0

# 运行 Docker 脚本
$REL_TOP/docker_run_sophonsdk.sh

# 在容器中编译
./build-bm1684-arm64.sh
```

## 环境变量

Docker 镜像中通常已设置好以下环境变量：

- `REL_TOP`: SophonSDK 根目录路径
- `BMNNSDK2_TOP`: 同 `REL_TOP`（备选）

如果未设置，需要手动设置：

```bash
export REL_TOP=/path/to/sophonsdk_v3.0.0
```

## 编译选项

```bash
# 基本编译
./build-bm1684-arm64.sh

# 指定构建目录
./build-bm1684-arm64.sh build/custom-dir

# 手动编译
mkdir -p build/bm1684-arm64-Release
cd build/bm1684-arm64-Release

cmake ../.. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_BM1684=ON \
    -DUSE_BM_FFMPEG=ON \
    -DUSE_BM_OPENCV=ON \
    -DTARGET_ARCH=arm64

cmake --build . -j$(nproc)
```

## 验证编译结果

```bash
# 检查可执行文件
file build/bm1684-arm64-Release/bin/detector_service

# 检查链接的库
ldd build/bm1684-arm64-Release/bin/detector_service | grep bm
```

## 工作方式说明

### 挂载 vs 复制

**使用挂载方式（推荐）**：
```bash
-v $(pwd):/workspace  # 挂载当前目录到容器
```
- 项目代码在宿主机上
- 容器中通过挂载访问
- 编译后的文件直接保存在宿主机的 `build/` 目录
- 容器删除后，所有文件都在宿主机上

**如果使用复制方式（不推荐）**：
```bash
# 需要在 Dockerfile 中 COPY 项目文件
# 编译后的文件在容器中，需要手动复制出来
docker cp container_id:/workspace/build ./build
```

### 文件位置

- **源代码**: 宿主机项目目录（如 `/Users/youngfuns/Documents/Project/yolo_service/`）
- **编译输出**: 宿主机项目目录下的 `build/` 目录
- **SDK**: Docker 镜像中（通过 `REL_TOP` 环境变量访问）

## 注意事项

1. **SDK 路径**: 确保 Docker 镜像中设置了正确的 `REL_TOP` 环境变量
2. **架构匹配**: 确保 Docker 镜像架构与目标平台匹配（arm64）
3. **依赖库**: Docker 镜像中应包含所有必要的编译工具和依赖
4. **文件权限**: 挂载的文件权限可能与容器内用户不匹配，如遇权限问题可添加 `-e LOCAL_USER_ID=$(id -u)` 参数

## 故障排除

### SDK 路径未找到

**错误**: `SophonSDK 路径无效: .../include 不存在`

**解决**: 检查并设置环境变量：
```bash
export REL_TOP=/path/to/sophonsdk_v3.0.0
echo $REL_TOP
```

### 库文件未找到

**错误**: 链接时找不到 `libbmrt.so` 等库

**解决**: 确保 `REL_TOP` 指向正确的 SDK 路径，并检查 `lib/bmnn/arm_pcie/` 目录是否存在

## 完整示例

### 步骤 1: 进入项目目录
```bash
cd /path/to/yolo_service
```

### 步骤 2: 启动 Docker 容器
```bash
docker run -it --rm \
    -v $(pwd):/workspace \
    -w /workspace \
    -e LOCAL_USER_ID=$(id -u) \
    sophgo/sophonsdk3:ubuntu18.04-py37-dev-22.06 \
    bash
```

### 步骤 3: 在容器中编译
```bash
# 检查 SDK 环境变量（通常已设置）
echo $REL_TOP

# 如果未设置，手动设置
# export REL_TOP=/path/to/sophonsdk_v3.0.0

# 编译
./build-bm1684-arm64.sh
```

### 步骤 4: 退出容器
```bash
exit
```

### 步骤 5: 在宿主机查看编译结果
```bash
# 编译后的文件在宿主机的 build/ 目录中
ls -lh build/bm1684-arm64-Release/bin/detector_service

# 可以直接在宿主机上运行（如果架构匹配）
./build/bm1684-arm64-Release/bin/detector_service
```

## 常见问题

### Q: 需要把整个项目复制到容器里吗？
**A**: 不需要。使用 `-v $(pwd):/workspace` 挂载即可，项目代码保留在宿主机。

### Q: 编译后的文件在哪里？
**A**: 在宿主机的 `build/` 目录中，与源代码在同一目录下。

### Q: 容器删除后文件会丢失吗？
**A**: 不会。所有文件都在宿主机上，容器只是提供编译环境。

### Q: 可以在宿主机编辑代码吗？
**A**: 可以。在宿主机编辑代码后，在容器中重新编译即可。
