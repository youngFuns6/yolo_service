# Linaro Ubuntu 20.04 交叉编译指南

本指南说明如何使用 Linaro 工具链交叉编译 detector_service 项目，目标平台为 ARM64 Ubuntu 20.04。

## 前置要求

### 1. 安装基础工具

```bash
# macOS
brew install cmake ninja

# Ubuntu/Debian
sudo apt-get install cmake ninja-build build-essential
```

### 2. 安装 Linaro 工具链

有两种方式安装 Linaro 工具链：

#### 方式一：使用系统包管理器（推荐，简单快速）

```bash
# Ubuntu/Debian
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# macOS (使用 Homebrew)
brew install aarch64-elf-gcc
```

#### 方式二：下载官方 Linaro 工具链（更完整，性能更好）

1. 下载 Linaro GCC 工具链：
```bash
# 下载 Linaro GCC 7.5.0（适用于 Ubuntu 20.04）
wget https://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/aarch64-linux-gnu/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu.tar.xz

# 解压
tar -xf gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu.tar.xz

# 设置环境变量
export LINARO_TOOLCHAIN_PATH=$(pwd)/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu
```

### 3. 创建 Ubuntu 20.04 sysroot（可选但推荐）

sysroot 包含目标系统的头文件和库文件，对于正确链接非常重要。

#### 使用 debootstrap 创建 sysroot

```bash
# 安装 debootstrap
sudo apt-get install debootstrap qemu-user-static binfmt-support

# 创建 sysroot 目录
sudo mkdir -p /opt/ubuntu20.04-rootfs

# 第一阶段：下载基础系统
sudo debootstrap --arch=arm64 --foreign focal /opt/ubuntu20.04-rootfs

# 第二阶段：完成安装（需要 qemu-user-static）
sudo cp /usr/bin/qemu-aarch64-static /opt/ubuntu20.04-rootfs/usr/bin/
sudo chroot /opt/ubuntu20.04-rootfs /debootstrap/debootstrap --second-stage

# 安装开发工具和库
sudo chroot /opt/ubuntu20.04-rootfs apt-get update
sudo chroot /opt/ubuntu20.04-rootfs apt-get install -y \
    build-essential \
    libssl-dev \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    pkg-config

# 设置环境变量
export LINARO_SYSROOT=/opt/ubuntu20.04-rootfs
```

#### 使用 Docker 创建 sysroot（替代方案）

```bash
# 运行 Ubuntu 20.04 ARM64 容器
docker run --rm -v /opt/ubuntu20.04-rootfs:/rootfs ubuntu:20.04 bash -c "
    apt-get update && \
    apt-get install -y build-essential libssl-dev libcurl4-openssl-dev libsqlite3-dev && \
    cp -r /usr/include /rootfs/usr/ && \
    cp -r /usr/lib /rootfs/usr/
"
```

## 使用方法

### 基本使用

```bash
# 使用默认配置构建
./build-linaro-ubuntu20.04.sh

# Debug 构建
./build-linaro-ubuntu20.04.sh -t Debug

# 指定工具链路径
./build-linaro-ubuntu20.04.sh --toolchain-path /opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu

# 指定 sysroot 路径
./build-linaro-ubuntu20.04.sh --sysroot /opt/ubuntu20.04-rootfs

# 清理构建目录
./build-linaro-ubuntu20.04.sh -c
```

### 使用环境变量

```bash
# 设置工具链路径
export LINARO_TOOLCHAIN_PATH=/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu

# 设置 sysroot 路径
export LINARO_SYSROOT=/opt/ubuntu20.04-rootfs

# 设置工具链前缀（如果需要）
export LINARO_TOOLCHAIN_PREFIX=aarch64-linux-gnu

# 运行构建
./build-linaro-ubuntu20.04.sh
```

### 完整示例

```bash
# 1. 设置环境变量
export LINARO_TOOLCHAIN_PATH=/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu
export LINARO_SYSROOT=/opt/ubuntu20.04-rootfs
export VCPKG_ROOT=$HOME/vcpkg

# 2. 运行构建
./build-linaro-ubuntu20.04.sh -t Release -j 8

# 3. 验证生成的二进制文件
file build/linaro-ubuntu20.04-Release/bin/detector_service
```

## 构建输出

构建完成后，可执行文件位于：
```
build/linaro-ubuntu20.04-Release/bin/detector_service
```

## 验证二进制文件

```bash
# 检查文件架构
file build/linaro-ubuntu20.04-Release/bin/detector_service

# 应该显示类似：
# detector_service: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), dynamically linked, ...

# 使用 readelf 查看详细信息
readelf -h build/linaro-ubuntu20.04-Release/bin/detector_service
```

## 在目标设备上运行

### 复制文件到目标设备

```bash
# 使用 scp 复制
scp build/linaro-ubuntu20.04-Release/bin/detector_service user@target-device:/usr/local/bin/

# 复制依赖库（如果需要）
scp -r build/linaro-ubuntu20.04-Release/lib/* user@target-device:/usr/local/lib/
```

### 在目标设备上设置环境

```bash
# SSH 到目标设备
ssh user@target-device

# 设置库路径
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# 运行程序
detector_service
```

## 常见问题

### 1. 找不到工具链

**错误信息：**
```
错误: 未找到 Linaro 工具链
```

**解决方法：**
- 确保已安装交叉编译工具链
- 或设置 `LINARO_TOOLCHAIN_PATH` 环境变量指向工具链安装目录

### 2. 链接错误：找不到库

**错误信息：**
```
/usr/bin/aarch64-linux-gnu-ld: cannot find -lxxx
```

**解决方法：**
- 创建并配置 sysroot
- 确保 sysroot 中包含所需的库文件
- 设置 `LINARO_SYSROOT` 环境变量

### 3. vcpkg 安装包失败

**错误信息：**
```
vcpkg install failed
```

**解决方法：**
- 检查网络连接
- 如果使用代理，确保代理设置正确
- 使用 `--no-proxy` 选项禁用代理
- 某些包可能需要较长时间编译，请耐心等待

### 4. CMake 配置失败

**错误信息：**
```
CMake 配置失败
```

**解决方法：**
- 检查 `VCPKG_ROOT` 环境变量是否正确设置
- 查看详细日志：`build/linaro-ubuntu20.04-Release/cmake_config.log`
- 确保工具链和 sysroot 路径正确

## 与标准构建脚本的区别

- `build.sh`: 通用的交叉编译脚本，支持多种平台和架构
- `build-linaro-ubuntu20.04.sh`: 专门针对 Linaro Ubuntu 20.04 的交叉编译脚本，提供更详细的工具链检测和配置

## 相关文件

- `build-linaro-ubuntu20.04.sh`: 主构建脚本
- `triplets/arm64-linux-linaro-ubuntu20.04.cmake`: vcpkg triplet 配置
- `toolchains/linaro-ubuntu20.04.cmake`: CMake 工具链文件

## 参考资源

- [Linaro 工具链下载](https://releases.linaro.org/components/toolchain/binaries/)
- [vcpkg 文档](https://github.com/Microsoft/vcpkg)
- [CMake 交叉编译指南](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling)
