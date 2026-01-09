# 构建说明

本文档说明如何使用构建脚本进行多平台交叉编译。

## 快速开始

### Linux/macOS

```bash
# 默认构建 (Linux x64 Release)
./build.sh

# 查看帮助
./build.sh -h

# 清理构建目录
./build.sh -c

# 指定平台和架构
./build.sh -p linux -a arm64

# Debug 构建
./build.sh -t Debug
```

### Windows

```cmd
REM 默认构建 (Windows x64 Release)
build.bat

REM 查看帮助
build.bat -h

REM 清理构建目录
build.bat -c

REM 指定平台和架构
build.bat -p windows -a x64

REM Debug 构建
build.bat -t Debug
```

## 支持的平台和架构

### Linux
- `linux-x64` - Linux x86_64
- `linux-x86` - Linux x86
- `linux-arm64` - Linux ARM64
- `linux-armv7` - Linux ARMv7

### Windows
- `windows-x64` - Windows x64
- `windows-x86` - Windows x86
- `windows-arm64` - Windows ARM64

### macOS
- `macos-x64` - macOS Intel
- `macos-arm64` - macOS Apple Silicon

### Android
- `android-arm64` - Android ARM64
- `android-armv7` - Android ARMv7

## 构建类型

- `Release` - 发布版本（默认，优化性能）
- `Debug` - 调试版本（包含调试信息）
- `RelWithDebInfo` - 带调试信息的发布版本

## 环境要求

### 必需

1. **CMake** (3.15+)
2. **vcpkg** - 包管理器
3. **C++17 兼容的编译器**

### vcpkg 设置

设置 `VCPKG_ROOT` 环境变量：

```bash
# Linux/macOS
export VCPKG_ROOT=$HOME/vcpkg

# Windows
set VCPKG_ROOT=C:\vcpkg
```

如果未设置，脚本会尝试在常见位置查找 vcpkg。

### Android 交叉编译

Android 交叉编译需要设置 `ANDROID_NDK` 环境变量：

```bash
export ANDROID_NDK=$HOME/Android/Sdk/ndk/25.1.8937393
```

## 构建示例

### 示例 1: Linux x64 Release

```bash
./build.sh -p linux -a x64 -t Release
```

输出目录: `build/linux-x64-Release/`

### 示例 2: Windows x64 Debug

```bash
# Linux/macOS 上交叉编译 Windows
./build.sh -p windows -a x64 -t Debug

# Windows 上本地编译
build.bat -p windows -a x64 -t Debug
```

输出目录: `build/windows-x64-Debug/`

### 示例 3: Linux ARM64

```bash
./build.sh -p linux -a arm64
```

输出目录: `build/linux-arm64-Release/`

### 示例 4: macOS Apple Silicon

```bash
./build.sh -p macos -a arm64
```

输出目录: `build/macos-arm64-Release/`

### 示例 5: 并行编译（使用 8 个线程）

```bash
./build.sh -j 8
```

## 输出结构

构建完成后，输出文件位于：

```
build/{platform}-{arch}-{type}/
├── bin/
│   └── detector_service[.exe]    # 可执行文件
└── lib/
    ├── libutils.so               # 工具库
    ├── libmodels.so              # 模型库
    ├── libdatabase.so            # 数据库库
    ├── libdetector.so            # 检测器库
    ├── libstream.so              # 推流库
    ├── libwebsocket.so           # WebSocket 库
    └── libapi.so                 # API 库
```

## 常见问题

### 1. vcpkg 未找到

**问题**: `错误: 未找到 vcpkg`

**解决**: 
- 设置 `VCPKG_ROOT` 环境变量
- 或确保 vcpkg 安装在 `$HOME/vcpkg` 或 `/usr/local/vcpkg`

### 2. 交叉编译工具链缺失

**问题**: CMake 配置失败，提示缺少工具链

**解决**: 
- 安装对应的交叉编译工具链
- 对于 Android，设置 `ANDROID_NDK`
- 确保 vcpkg 已安装对应 triplet 的依赖

### 3. 依赖包未安装

**问题**: 编译时找不到依赖库

**解决**: 
```bash
# 安装依赖（以 Linux x64 为例）
vcpkg install crow sqlite3 onnxruntime opencv4[ffmpeg] nlohmann-json --triplet x64-linux
```

### 4. 内存不足

**问题**: 编译时内存不足

**解决**: 
- 减少并行任务数: `./build.sh -j 2`
- 或使用单线程编译: `./build.sh -j 1`

## 高级用法

### 自定义构建目录

修改脚本中的 `BUILD_DIR` 变量，或直接使用 CMake：

```bash
mkdir -p custom_build
cd custom_build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-linux \
        -DCMAKE_BUILD_TYPE=Release
cmake --build . -j 8
```

### 只编译特定模块

使用 CMake 的 `--target` 选项：

```bash
cd build/linux-x64-Release
cmake --build . --target utils -j 8
```

## 清理构建

```bash
# 清理所有构建文件
./build.sh -c

# 或手动删除
rm -rf build/
```

## 持续集成 (CI)

### GitHub Actions 示例

```yaml
- name: Build Linux x64
  run: |
    export VCPKG_ROOT=$HOME/vcpkg
    ./build.sh -p linux -a x64 -t Release
```

### GitLab CI 示例

```yaml
build:
  script:
    - export VCPKG_ROOT=$CI_PROJECT_DIR/vcpkg
    - ./build.sh -p linux -a x64 -t Release
```

## 故障排除

如果遇到问题：

1. 检查 `VCPKG_ROOT` 环境变量
2. 确认 vcpkg 已安装所需依赖
3. 查看 CMake 配置输出
4. 检查编译器版本和兼容性
5. 查看构建日志中的错误信息

## 相关文档

- [README.md](README.md) - 项目使用说明
- [vcpkg.json](vcpkg.json) - 依赖配置
- [CMakeLists.txt](CMakeLists.txt) - CMake 配置

