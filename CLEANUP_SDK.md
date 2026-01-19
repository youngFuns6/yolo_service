# SophonSDK 清理指南

本指南说明如何清理 SophonSDK，只保留编译和运行所需的必要文件。

## 清理脚本

使用 `cleanup_sophonsdk.sh` 脚本可以自动清理不需要的文件。

### 使用方法

```bash
# 保留所有（默认，包括 FFmpeg 和 OpenCV）
./cleanup_sophonsdk.sh

# 或明确指定
./cleanup_sophonsdk.sh yes yes

# 只保留核心库，删除 FFmpeg 和 OpenCV（最小配置）
./cleanup_sophonsdk.sh no no

# 保留 FFmpeg，删除 OpenCV
./cleanup_sophonsdk.sh yes no

# 删除 FFmpeg，保留 OpenCV
./cleanup_sophonsdk.sh no yes
```

### 参数说明

- 第一个参数: 是否保留 FFmpeg 和 decode 库（yes/no，默认 yes）
- 第二个参数: 是否保留 OpenCV 库（yes/no，默认 yes）

## 清理内容

### 删除的根目录

- `bin/` - 工具程序（534M）
- `bmnet/` - 模型转换工具（239M）
- `bmlang/` - 语言工具（7.1M）
- `driver/` - 驱动文件（71M）
- `install/` - 安装脚本
- `res/` - 资源文件（424K）
- `scripts/` - 脚本（128K）
- `test/` - 测试文件（952K）
- `bm1684_firmware/` - 固件（6.0M）
- `bm1684x_firmware/` - 固件（1.9M）

### 删除的 lib 子目录

- `bmcompiler/` - 模型编译工具（100M）
- `bmlang/` - 语言工具（17M）
- `calibration-tools/` - 校准工具（11M）
- `sys/` - 系统库（15M）
- `vpp/` - 视频处理（2.8M）

### 删除的 include 子目录

- `bmcompiler/` - 编译工具头文件（432K）
- `bmnetc/` - 模型转换工具头文件（1.9M）
- `bmnetu/` - 模型转换工具头文件
- `bmlang/` - 语言工具头文件
- `bmcpu/` - CPU 相关头文件
- `bmlog/` - 日志头文件
- `calibration/` - 校准工具头文件
- `ufw/` - UFW 头文件（3.4M）
- `vpp/` - 视频处理头文件

## 保留的内容

### 必需保留（核心功能）

- `include/bmruntime/` - BMRuntime 头文件（128K）
- `include/bmlib/` - BMLib 头文件（104K）
- `include/third_party/` - 第三方头文件（200M）
- `lib/bmnn/` - 核心推理库（66M）
- `lib/thirdparty/` - 第三方依赖库（1.2G）

### 推荐保留

- `include/sail/` - Sail 头文件（692K）
- `lib/sail/` - Sail 推理库（148M）

### 可选保留（根据需求）

- `include/ffmpeg/` - FFmpeg 头文件（1.2M）
- `include/decode/` - 解码头文件（652K）
- `lib/ffmpeg/` - FFmpeg 库（1.8G）
- `lib/decode/` - 解码库（17M）
- `include/opencv/` - OpenCV 头文件（4.8M）
- `lib/opencv/` - OpenCV 库（1.5G）

## 清理前后大小对比

### 清理前
- 总大小: 约 5.5GB

### 清理后（最小配置）
- 只保留核心库: 约 1.5GB
- 节省空间: 约 4GB

### 清理后（保留 FFmpeg）
- 保留核心库 + FFmpeg: 约 3.3GB
- 节省空间: 约 2.2GB

### 清理后（保留所有）
- 保留核心库 + FFmpeg + OpenCV: 约 4.8GB
- 节省空间: 约 700MB

## 建议配置

### 最小配置（推荐用于生产环境）

如果只需要 TPU 推理功能，不需要硬件编解码：

```bash
./cleanup_sophonsdk.sh no no
```

保留内容：
- 核心库（bmnn, thirdparty）
- Sail 库（推荐）
- 头文件（bmruntime, bmlib, sail, third_party）

### 完整配置（推荐用于开发环境）

如果需要硬件编解码和完整功能：

```bash
./cleanup_sophonsdk.sh yes yes
```

保留内容：
- 所有核心库
- FFmpeg 和 decode 库
- OpenCV 库
- 所有必要的头文件

## 注意事项

1. **备份**: 清理前建议备份原始 SDK
2. **不可逆**: 清理操作不可逆，请谨慎操作
3. **编译需求**: 如果后续需要模型转换工具，需要重新解压 SDK
4. **固件**: 固件文件通常不需要在编译时保留，但运行时可能需要

## 验证清理结果

清理后，可以验证必要的库文件是否存在：

```bash
# 检查核心库
ls third_party/sophonsdk_v3.0.0/lib/bmnn/soc/*.so

# 检查 Sail 库
ls third_party/sophonsdk_v3.0.0/lib/sail/soc/*.so

# 检查头文件
ls third_party/sophonsdk_v3.0.0/include/bmruntime/
ls third_party/sophonsdk_v3.0.0/include/sail/
```
