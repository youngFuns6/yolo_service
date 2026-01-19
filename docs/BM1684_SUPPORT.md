# BM1684平台支持说明

本文档说明如何在detector_service中使用算能BM1684平台的硬件编解码和TPU推理功能。

## 功能特性

- **硬件视频解码**: 使用BM1684硬件解码器（h264_bm, h265_bm等）进行视频流解码
- **硬件视频编码**: 使用BM1684硬件编码器（h264_bm, h265_bm等）进行视频流编码，特别适用于GB28181推流
- **TPU推理**: 使用BMRuntime进行YOLO模型的TPU推理，大幅提升推理性能
- **BMCV图像处理**: 使用BMCV进行硬件加速的图像预处理

## 编译要求

### 1. 安装SophonSDK

确保已安装SophonSDK（BMNNSDK2），并设置环境变量：

```bash
export BMNNSDK2_TOP=/path/to/bmnnsdk2
```

或者将SDK安装在默认位置：`$HOME/bmnnsdk2/bmnnsdk2-latest`

### 2. 编译配置

#### 方法1: 使用build.sh脚本（推荐）

启用BM1684支持：

```bash
# 设置BMNNSDK2环境变量（如果未安装在默认位置）
export BMNNSDK2_TOP=/path/to/bmnnsdk2

# 使用build.sh脚本编译
./build.sh --bm1684

# 或者指定平台和架构
./build.sh -p linux -a arm64 --bm1684
```

#### 方法2: 使用CMake直接编译

```bash
# 设置BMNNSDK2环境变量
export BMNNSDK2_TOP=/path/to/bmnnsdk2

# 配置CMake
cmake -DENABLE_BM1684=ON ..

# 编译
make
```

#### 环境变量说明

- `BMNNSDK2_TOP`: BMNNSDK2 SDK的安装路径
  - 默认位置: `$HOME/bmnnsdk2/bmnnsdk2-latest`
  - 如果SDK安装在默认位置，可以不设置此环境变量

## 模型准备

BM1684平台需要使用BModel格式的模型文件，而不是ONNX格式。

### 模型转换

1. 使用NNToolChain将ONNX模型转换为BModel：

```bash
# 使用bm_model_tool或bm_model_convert工具
bm_model_convert --model=your_model.onnx --output=your_model.bmodel
```

2. 将转换后的BModel文件放在models目录下

### 配置文件

在配置文件中指定BModel路径：

```json
{
  "detector": {
    "model_path": "models/yolov11n.bmodel",
    "execution_provider": "BM1684",
    "device_id": 0,
    "use_bm1684_hw_decode": true,
    "bm1684_codec_name": "h264_bm",
    "bm1684_sophon_idx": 0,
    "bm1684_pcie_no_copyback": 0
  }
}
```

## 使用方法

### 1. 使用BM1684检测器

在代码中使用BM1684检测器：

```cpp
#ifdef ENABLE_BM1684
#include "yolov11_detector_bm1684.h"

auto detector = std::make_shared<YOLOv11DetectorBM1684>(
    "models/yolov11n.bmodel",
    0.65f,  // conf_threshold
    0.45f,  // nms_threshold
    640,    // input_width
    640,    // input_height
    0       // device_id
);

if (detector->initialize()) {
    auto detections = detector->detect(image);
    // 处理检测结果
}
#endif
```

### 2. 使用BM1684视频解码器

```cpp
#ifdef ENABLE_BM1684
#include "bm1684_video_decoder.h"

BM1684VideoDecoder decoder;
if (decoder.open("rtsp://example.com/stream", "h264_bm", 0, 0)) {
    cv::Mat frame;
    while (decoder.read(frame)) {
        // 处理帧
    }
    decoder.close();
}
#endif
```

### 3. 使用BM1684视频编码器

```cpp
#ifdef ENABLE_BM1684
#include "bm1684_video_encoder.h"

BM1684VideoEncoder encoder;
if (encoder.initialize("h264_bm", 1920, 1080, 25, 2000000, 50, 0, false)) {
    cv::Mat frame;
    // ... 获取视频帧 ...
    
    // 编码帧
    if (encoder.encodeFrame(frame)) {
        AVPacket* pkt = av_packet_alloc();
        while (encoder.getEncodedPacket(pkt)) {
            // 处理编码后的数据包
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    
    encoder.close();
}
#endif
```

### 4. 在StreamManager中使用（自动硬件编解码）

StreamManager会自动根据配置选择使用BM1684或标准解码器：

```cpp
auto& config = Config::getInstance();
const auto& detector_config = config.getDetectorConfig();

if (detector_config.execution_provider == ExecutionProvider::BM1684) {
    // 使用BM1684检测器和解码器
    auto detector = std::make_shared<YOLOv11DetectorBM1684>(...);
    stream_manager.startAnalysisBM1684(channel_id, channel, detector);
} else {
    // 使用标准检测器和解码器
    auto detector = std::make_shared<YOLOv11Detector>(...);
    stream_manager.startAnalysis(channel_id, channel, detector);
}
```

## 配置参数说明

### DetectorConfig中的BM1684参数

- `use_bm1684_hw_decode`: 是否使用BM1684硬件解码（默认: true）
- `bm1684_codec_name`: 解码器名称，可选值：
  - `"h264_bm"`: H.264硬件解码器
  - `"h265_bm"`: H.265硬件解码器
  - 其他BM1684支持的编解码器
- `bm1684_sophon_idx`: BM1684设备索引（默认: 0）
- `bm1684_pcie_no_copyback`: PCIe模式下是否启用零拷贝（默认: 0）

## 性能优化建议

1. **使用硬件解码**: 启用`use_bm1684_hw_decode`以利用硬件解码加速
2. **使用硬件编码**: GB28181推流时自动使用硬件编码（当execution_provider为BM1684时）
3. **零拷贝模式**: 在PCIe模式下启用`bm1684_pcie_no_copyback`可以减少内存拷贝
4. **批量处理**: 如果支持，使用批量推理以提高吞吐量
5. **BMCV预处理**: 使用BMCV进行图像预处理，减少CPU负担
6. **硬件编解码链路**: 完整的硬件编解码链路可以大幅降低CPU占用，提升整体性能

## 注意事项

1. **模型格式**: BM1684需要使用BModel格式，不是ONNX格式
2. **SDK版本**: 确保使用的SophonSDK版本与示例代码兼容
3. **设备检测**: 在运行时检测BM1684设备是否可用
4. **回退机制**: 如果BM1684不可用，代码会自动回退到CPU模式

## 故障排除

### 问题1: 找不到BMNNSDK2

**解决方案**: 
- 设置环境变量`BMNNSDK2_TOP`指向SDK安装目录
- 或者将SDK安装在默认位置: `$HOME/bmnnsdk2/bmnnsdk2-latest`
- 使用build.sh脚本时会自动检测SDK位置

### 问题2: 模型加载失败

**解决方案**: 
- 确认模型文件路径正确
- 确认模型是BModel格式
- 检查模型是否与BM1684兼容

### 问题3: 硬件解码失败

**解决方案**:
- 检查视频流格式是否支持
- 确认BM1684设备正常工作
- 尝试使用软件解码作为回退

### 问题4: 硬件编码失败

**解决方案**:
- 确认BM1684设备正常工作
- 检查编码参数（分辨率、帧率、比特率）是否在硬件支持范围内
- 确认GB28181推流配置正确
- 如果硬件编码失败，系统会自动回退到软件编码

### 问题5: vcpkg 编译器检测失败（GCC 7.4.1 等版本）

**症状**: 使用某些版本的交叉编译器（如 gcc-linaro-7.4.1-2019.02）时，vcpkg 在 detect_compiler 阶段失败。

**解决方案**:
1. **使用兼容的编译器版本**: 推荐使用 gcc-linaro-6.3.1-2017.05 或更新的版本（GCC 11+）
2. **确保编译器路径正确**: 检查环境变量 `CMAKE_C_COMPILER` 和 `CMAKE_CXX_COMPILER` 是否指向正确的编译器路径
3. **验证编译器可执行**: 运行 `$CMAKE_C_COMPILER --version` 确认编译器可以正常执行
4. **检查 triplet 配置**: 确保 `triplets/arm64-linux.cmake` 文件包含正确的编译器配置
5. **清理构建缓存**: 如果问题持续，尝试清理 vcpkg 的构建缓存：
   ```bash
   rm -rf build/linux-arm64-Release/vcpkg_installed
   rm -rf vcpkg/buildtrees/detect_compiler
   ```

**技术说明**: 
- vcpkg 在 detect_compiler 阶段会尝试检测编译器信息
- 某些版本的 GCC（特别是 7.x 系列）可能与 vcpkg 的检测机制不兼容
- triplet 文件已配置 `CMAKE_C_COMPILER_WORKS` 和 `CMAKE_CXX_COMPILER_WORKS` 来跳过编译器检测
- 如果问题仍然存在，建议使用 GCC 6.3.1 或 GCC 11+ 版本

## 参考资源

- [SophonSDK文档](https://sophgo-doc.gitbook.io/sophonsdk3/)
- [BM1684示例代码](../examples/)
- [NNToolChain模型转换工具](../examples/nntc/)

