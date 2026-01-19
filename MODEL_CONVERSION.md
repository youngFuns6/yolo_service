# 模型转换指南

## 概述

本项目使用 YOLOv11 模型进行目标检测。在 BM1684 平台上，需要将 ONNX 模型转换为 bmodel 格式才能使用 TPU 进行推理。

## 模型文件

- **ONNX 模型**: `models/yolov11n.onnx` (10MB)
- **bmodel 模型**: 转换后生成 `models/yolov11n.bmodel`

## 转换环境要求

### ⚠️ 重要提示

**模型转换必须在 BM1684 开发环境中进行**，因为：

1. bmneto 需要 onnxruntime 1.6.0，该版本已从 PyPI 移除
2. BM1684 开发环境已预装正确版本的依赖
3. 转换工具需要访问 TPU 设备进行优化

### 环境设置

在 BM1684 开发环境中：

```bash
# 1. 设置 SDK 环境
export REL_TOP=/path/to/sophonsdk_v3.0.0
source $REL_TOP/scripts/envsetup.sh

# 2. 验证环境
python3 -c "import bmneto; print('bmneto 可用')"
```

## 转换步骤

### 方式 1: 使用快速脚本（推荐）

```bash
cd /path/to/yolo_service
./scripts/convert_yolov11n.sh
```

### 方式 2: 使用完整脚本

```bash
./scripts/convert_onnx_to_bmodel_bm1684.sh \
    models/yolov11n.onnx \
    models/yolov11n_bmodel \
    BM1684 \
    "[1,3,640,640]"
```

### 方式 3: 使用 Python 脚本

```bash
python3 scripts/convert_onnx_to_bmodel.py \
    models/yolov11n.onnx \
    models/yolov11n_bmodel \
    BM1684 \
    1 640 640
```

## 转换参数说明

- **输入文件**: `models/yolov11n.onnx`
- **输出目录**: `models/yolov11n_bmodel/`
- **目标设备**: `BM1684`
- **输入形状**: `[1, 3, 640, 640]` (batch=1, channels=3, height=640, width=640)

## 转换输出

转换成功后，会生成：

```
models/yolov11n_bmodel/
├── yolov11n/
│   ├── compilation/          # 编译过程文件
│   └── yolov11n.bmodel      # 编译后的模型文件
└── yolov11n.bmodel          # 复制到 models 目录的模型文件
```

## 验证转换结果

```bash
# 使用 bmrt_test 验证模型
bmrt_test --context_dir=models/yolov11n_bmodel/yolov11n
```

## 使用转换后的模型

在代码中，将模型路径设置为 bmodel 文件：

```cpp
// 使用 bmodel 进行 TPU 推理
YOLOv11Detector detector("models/yolov11n.bmodel", ...);
```

## 故障排除

### 1. bmneto 未找到

**错误**: `ModuleNotFoundError: No module named 'bmneto'`

**解决方法**:
```bash
export REL_TOP=/path/to/sophonsdk_v3.0.0
source $REL_TOP/scripts/envsetup.sh
```

### 2. onnxruntime 版本不兼容

**错误**: `ERROR: Could not find a version that satisfies the requirement onnxruntime==1.6.0`

**解决方法**: 在 BM1684 开发环境中转换，该环境已预装正确版本

### 3. 转换失败

检查：
- ONNX 模型文件是否有效
- 输入形状是否正确（YOLOv11 默认 640x640）
- 目标设备是否正确（BM1684）

## 模型优化选项

转换时可以调整优化参数：

- `--opt=2`: 优化等级（0=无优化, 1=基本优化, 2=完全优化）
- `--dyn=false`: 动态编译（false=静态形状，性能更好）

## 参考

- SophonSDK 文档: https://sophgo-doc.gitbook.io/sophonsdk/
- bmneto 使用说明: `examples/nntc/bmneto/README.md`
