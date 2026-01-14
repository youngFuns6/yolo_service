# 修复 ONNX Schema 重复注册错误

## 问题描述

在加载 ONNX 模型时，可能会出现以下警告信息：

```
Schema error: Trying to register schema with name TreeEnsembleRegressor (domain: ai.onnx.ml version: 5)
Schema error: Trying to register schema with name TreeEnsembleClassifier (domain: ai.onnx.ml version: 5)
```

这些是警告信息，不会导致程序崩溃，但会在控制台输出错误信息。

## 根本原因

ONNX 库在静态初始化时会注册 schema，当多个地方尝试注册相同的 schema 时会出现此警告。这通常发生在：
- ONNX Runtime 依赖的 ONNX 库版本问题
- ONNX 库的 schema 注册机制在重复注册时默认会报错

## 解决方案（推荐）

项目已经包含了修复 patch 文件 `fix-schema-duplicate.patch`，它会将 `fail_duplicate_schema` 参数从 `true` 改为 `false`，从而允许重复注册而不报错。

### 方法 1：使用自动化脚本（最简单）

```bash
# 运行脚本自动应用 patch
./apply_onnx_patch.sh
```

脚本会自动：
1. 移除已安装的 onnxruntime 和 onnx
2. 使用 overlay-ports 重新安装 onnx（应用 patch）
3. 重新安装 onnxruntime

### 方法 2：手动执行

```bash
# 1. 进入 vcpkg 目录
cd $VCPKG_ROOT

# 2. 移除已安装的包
./vcpkg remove onnxruntime --triplet arm64-osx
./vcpkg remove onnx --triplet arm64-osx

# 3. 使用 overlay-ports 重新安装 onnx（应用 patch）
./vcpkg install onnx --triplet arm64-osx --overlay-ports=/Users/youngfuns/Documents/Project/yolo_service/ports

# 4. 重新安装 onnxruntime
./vcpkg install onnxruntime --triplet arm64-osx
```

### 方法 3：使用 CMake 配置（如果使用 manifest 模式）

如果项目使用 vcpkg manifest 模式（vcpkg.json），CMakeLists.txt 已经配置了 overlay-ports：

```bash
cd build/macos-arm64-Release
rm -rf *
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
make
```

CMake 配置时会自动使用 overlay-ports，vcpkg 会应用 patch 重新安装 onnx。

## 验证修复

应用 patch 后，重新编译并运行程序：

```bash
cd build/macos-arm64-Release
make
./bin/detector_service
```

如果不再看到 schema 重复注册的错误信息，说明修复成功。

## Patch 文件说明

项目包含以下 patch 文件（位于 `ports/onnx/` 目录）：
- `fix-schema-duplicate.patch` - **修复 schema 重复注册问题**（核心修复）
- `fix-pr-7390.patch` - 修复 PR #7390 相关问题
- `fix-cmakelists.patch` - 修复 CMakeLists.txt 问题

`fix-schema-duplicate.patch` 会将以下函数的 `fail_duplicate_schema` 参数默认值从 `true` 改为 `false`：
- `OpSchemaRegisterOnce::OpSchemaRegisterOnce()`
- `OpSchemaRegisterOnce::OpSchemaRegisterNoExcept()`
- `OpSchemaRegisterOnce::OpSchemaRegisterImpl()`

## 技术细节

### Overlay Ports 工作原理

vcpkg 的 overlay-ports 功能允许我们覆盖标准 port 的配置。当使用 `--overlay-ports` 参数时，vcpkg 会：
1. 首先查找 overlay-ports 目录中的 port
2. 如果找到，使用 overlay 中的 portfile.cmake 和 patch 文件
3. 如果未找到，回退到标准 vcpkg ports 目录

### CMakeLists.txt 配置

项目已经在 CMakeLists.txt 中配置了 overlay-ports：

```cmake
set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/ports" CACHE STRING "VCPKG overlay ports")
```

这确保在 CMake 配置时，vcpkg 会自动使用项目中的 overlay ports。

## 注意事项

1. **重新编译时间**：重新安装 onnxruntime 可能需要较长时间（10-30 分钟），因为它需要重新编译 ONNX 库
2. **依赖关系**：onnxruntime 依赖 onnx，所以需要先重新安装 onnx，再安装 onnxruntime
3. **Triplet 匹配**：确保使用的 triplet 与项目构建时使用的 triplet 一致（通常是 `arm64-osx` 或 `x64-osx`）

## 如果仍有问题

如果应用 patch 后仍然出现错误，请检查：
1. patch 文件是否正确应用（检查 vcpkg 构建日志）
2. 是否使用了正确的 overlay-ports 路径
3. 是否重新编译了项目（不仅仅是重新链接）
