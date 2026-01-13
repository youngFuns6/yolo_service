# eXosip2 从源码编译说明

## 概述

本项目已配置为自动从源码编译 eXosip2 和 osip2 库，支持跨平台构建（Linux、Windows、macOS）。

## 自动编译

CMake 配置会自动：
1. 从 GitLab 下载 osip2 和 eXosip2 源码
2. 使用 autotools (configure/make) 编译
3. 安装到构建目录的 `third_party/exosip2-install`
4. 自动链接到项目

## 前置要求

### Linux
```bash
# 安装 autotools 和构建工具
sudo apt-get install -y \
    build-essential \
    autoconf \
    automake \
    libtool \
    git \
    pkg-config
```

### Windows (MinGW/MSYS2)
```bash
# 使用 MSYS2
pacman -S base-devel autoconf automake libtool git pkg-config
```

### macOS
```bash
# 使用 Homebrew
brew install autoconf automake libtool pkg-config
```

## 构建步骤

### 1. 正常构建项目

```bash
./build.sh
```

CMake 会自动：
- 检测是否已安装 eXosip2（通过 pkg-config）
- 如果未找到，自动从源码编译
- 编译过程会显示在日志中

### 2. 强制从源码编译

如果你想强制从源码编译（即使系统已安装）：

```bash
cd build/linux-x64-Release  # 或你的构建目录
cmake .. -DEXOSIP2_BUILD_FROM_SOURCE=ON
cmake --build .
```

### 3. 使用系统已安装的库

如果你想使用系统已安装的库（跳过源码编译）：

```bash
cd build/linux-x64-Release
cmake .. -DEXOSIP2_BUILD_FROM_SOURCE=OFF
cmake --build .
```

## 编译过程

首次构建时，CMake 会：

1. **下载 osip2 源码** (~5-10分钟，取决于网络)
   ```
   -- Downloading osip2 from GitLab...
   ```

2. **配置 osip2**
   ```
   -- Configuring osip2...
   ```

3. **编译 osip2**
   ```
   -- Building osip2...
   ```

4. **安装 osip2**
   ```
   -- Installing osip2...
   ```

5. **下载 eXosip2 源码**
   ```
   -- Downloading eXosip2 from GitLab...
   ```

6. **配置 eXosip2**（依赖 osip2）
   ```
   -- Configuring eXosip2...
   ```

7. **编译 eXosip2**
   ```
   -- Building eXosip2...
   ```

8. **安装 eXosip2**
   ```
   -- Installing eXosip2...
   ```

9. **编译主项目**
   ```
   -- Building detector_service...
   ```

## 编译位置

编译后的库位于：
```
build/linux-x64-Release/third_party/exosip2-install/
├── include/
│   ├── eXosip2/
│   └── osip2/
└── lib/
    ├── libeXosip2.a
    ├── libosip2.a
    └── libosipparser2.a
```

## 常见问题

### 问题1: Git 下载失败

**错误信息**:
```
fatal: unable to access 'https://gitlab.com/...': SSL certificate problem
```

**解决方法**:
```bash
# 临时禁用 SSL 验证（不推荐，仅用于测试）
git config --global http.sslVerify false

# 或使用代理
git config --global http.proxy http://proxy.example.com:8080
```

### 问题2: configure 失败

**错误信息**:
```
configure: error: C compiler cannot create executables
```

**解决方法**:
- 检查编译器是否正确安装
- 检查环境变量 PATH
- 在 Windows 上确保使用 MSYS2 或 MinGW

### 问题3: make 失败

**错误信息**:
```
make: *** [all] Error 2
```

**解决方法**:
- 查看详细错误日志：`build/.../third_party/osip2-build/CMakeFiles/osip2-build.log`
- 检查是否缺少依赖库
- 尝试单线程编译：`make -j1`

### 问题4: 网络问题

如果 GitLab 访问困难，可以：

1. **使用镜像源**（需要修改 cmake/FindExosip2.cmake）
2. **手动下载源码**：
   ```bash
   cd build/third_party
   git clone https://gitlab.com/sipwise/libosip2.git osip2
   git clone https://gitlab.com/sipwise/libexosip2.git exosip2
   ```
   然后重新运行 CMake

## 清理编译产物

```bash
# 清理第三方库
rm -rf build/*/third_party/

# 完全清理
rm -rf build/
```

## 跨平台支持

### Linux
- ✅ 完全支持
- 使用 autotools (configure/make)

### Windows (MinGW)
- ✅ 支持
- 需要 MSYS2 环境
- 使用 autotools

### Windows (MSVC)
- ⚠️ 部分支持
- 可能需要修改构建脚本
- 建议使用 MinGW

### macOS
- ✅ 完全支持
- 使用 autotools
- 需要 Xcode Command Line Tools

## 性能优化

### 并行编译
CMake 会自动使用所有 CPU 核心：
```bash
cmake --build . -j$(nproc)  # Linux
cmake --build . -j$(sysctl -n hw.ncpu)  # macOS
```

### 缓存编译产物
编译后的库会缓存在构建目录，下次构建时不会重新编译（除非清理）。

## 验证安装

构建成功后，检查日志：
```
-- eXosip2 will be built from source
--   Install dir: .../third_party/exosip2-install
--   Include dirs: ...
--   Libraries: ...
```

检查库文件：
```bash
ls -lh build/*/third_party/exosip2-install/lib/
# 应该看到：
# libeXosip2.a
# libosip2.a
# libosipparser2.a
```

## 故障排查

### 查看详细日志
```bash
# 查看下载日志
cat build/*/third_party/osip2-download.log

# 查看配置日志
cat build/*/third_party/osip2-build/CMakeFiles/osip2-configure.log

# 查看编译日志
cat build/*/third_party/osip2-build/CMakeFiles/osip2-build.log
```

### 手动测试编译
```bash
cd build/third_party/osip2
./configure --prefix=$(pwd)/../exosip2-install --enable-static --disable-shared
make -j$(nproc)
make install
```

## 相关文档

- [GB28181_README.md](GB28181_README.md) - GB28181功能说明
- [BUILD_GB28181.md](BUILD_GB28181.md) - 构建指南
- [GB28181_SIP_IMPLEMENTATION.md](GB28181_SIP_IMPLEMENTATION.md) - 实现细节

