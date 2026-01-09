# 视觉分析服务

基于 Crow 框架和 YOLOv11 的视觉分析服务，支持视频流分析、推流、WebSocket 实时传输和报警记录存储。

## 功能特性

1. **通道管理**：支持视频通道的增删改查操作
2. **视频推流**：支持将分析后的视频推流到 RTMP 服务器
3. **WebSocket 传输**：实时传输报警信息和绘制好的图片帧
4. **报警存储**：使用 SQLite 存储报警图片和相关信息
5. **YOLOv11 检测**：使用 YOLOv11 模型进行目标检测

## 项目结构

```
detector_service/
├── src/
│   ├── main.cpp                 # 主服务入口
│   ├── config/                  # 配置模块
│   │   └── config.h
│   ├── models/                  # 数据模型
│   │   ├── channel.h
│   │   └── channel.cpp
│   ├── database/                # 数据库模块
│   │   ├── database.h
│   │   └── database.cpp
│   ├── detector/                # 检测器模块
│   │   ├── yolov11_detector.h
│   │   └── yolov11_detector.cpp
│   ├── stream/                  # 推流模块
│   │   ├── stream_manager.h
│   │   └── stream_manager.cpp
│   ├── websocket/               # WebSocket 模块
│   │   ├── ws_handler.h
│   │   └── ws_handler.cpp
│   ├── api/                     # API 路由
│   │   ├── channel_api.h
│   │   ├── channel_api.cpp
│   │   ├── alert_api.h
│   │   └── alert_api.cpp
│   └── utils/                   # 工具函数
│       ├── image_utils.h
│       └── image_utils.cpp
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

## 依赖项

- Crow (HTTP/WebSocket 服务器)
- OpenCV4 (图像处理)
- ONNX Runtime (模型推理)
- SQLite3 (数据库)
- FFmpeg (视频推流)
- nlohmann-json (JSON 处理)

## 构建说明

### 前置要求

1. 安装 vcpkg
2. 安装 CMake (3.15+)
3. 安装 C++17 兼容的编译器

### 构建步骤

```bash
# 设置 vcpkg 环境变量
export VCPKG_ROOT=/path/to/vcpkg

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build .

# 运行
./bin/detector_service
```

## 使用说明

### 1. 准备 YOLOv11 模型

将 YOLOv11 ONNX 模型文件放置在项目根目录，命名为 `yolov11n.onnx`，或修改 `main.cpp` 中的模型路径。

### 2. 启动服务

```bash
./bin/detector_service
```

服务将在 `http://localhost:8080` 启动。

### 3. API 接口

#### 通道管理

- **创建通道**：`POST /api/channels`
  ```json
  {
    "name": "通道1",
    "source_url": "rtsp://example.com/stream",
    "rtmp_url": "rtmp://example.com/live/stream",
    "width": 1920,
    "height": 1080,
    "fps": 25,
    "enabled": true
  }
  ```

- **获取所有通道**：`GET /api/channels`

- **获取单个通道**：`GET /api/channels/{id}`

- **更新通道**：`PUT /api/channels/{id}`

- **删除通道**：`DELETE /api/channels/{id}`

- **启动通道**：`POST /api/channels/{id}/start`

- **停止通道**：`POST /api/channels/{id}/stop`

#### 报警记录

- **获取所有报警**：`GET /api/alerts?limit=100&offset=0`

- **获取单个报警**：`GET /api/alerts/{id}`

- **获取通道报警**：`GET /api/channels/{id}/alerts`

- **删除报警**：`DELETE /api/alerts/{id}`

### 4. WebSocket 连接

连接到 `ws://localhost:8080/ws` 以接收：

- **报警信息**：JSON 格式，包含检测结果和图片
- **图片帧**：实时传输分析后的视频帧

消息格式：

```json
{
  "type": "alert",
  "channel_id": 1,
  "channel_name": "通道1",
  "alert_type": "detection",
  "image_base64": "...",
  "confidence": 0.95,
  "detected_objects": "[{...}]",
  "timestamp": "2024-01-01 12:00:00"
}
```

```json
{
  "type": "frame",
  "channel_id": 1,
  "image_base64": "...",
  "timestamp": "2024-01-01 12:00:00"
}
```

## 配置

可以在 `main.cpp` 中修改配置：

- 检测器配置：模型路径、置信度阈值等
- 数据库配置：数据库路径、存储天数等
- 服务器配置：端口号、WebSocket 路径等

## 注意事项

1. 确保 YOLOv11 模型文件存在且格式正确
2. RTMP 推流地址需要可访问
3. 视频源地址需要可访问
4. 报警图片存储在 `alerts/` 目录
5. 数据库文件为 `alerts.db`

## 许可证

MIT License

