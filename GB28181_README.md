# GB28181集成说明

## 概述

本项目已集成GB28181国标协议推流功能，可将视频分析结果推送到符合GB28181标准的上级平台（如视频监控平台）。

## 功能特点

- ✅ 支持PS流和H.264流格式
- ✅ 基于FFmpeg的RTP推流
- ✅ 每个通道独立的GB28181推流控制
- ✅ 完整的配置管理API
- ✅ 数据库持久化配置
- ✅ 被动推流模式（由上级平台控制）
- ✅ **完整的SIP信令交互**（新增）
  - SIP注册与认证
  - 心跳保活
  - 自动响应Invite请求
  - 目录查询响应
  - 设备信息查询响应
  - 设备状态查询响应

## 架构说明

### 核心组件

1. **GB28181Config** (`src/models/include/gb28181_config.h`)
   - GB28181配置管理
   - 包含SIP服务器、设备信息、端口配置等

2. **GB28181Streamer** (`src/stream/include/gb28181_streamer.h`)
   - RTP推流器
   - 支持PS/H.264格式
   - 基于FFmpeg实现

3. **GB28181SipClient** (`src/stream/include/gb28181_sip_client.h`) ⭐新增
   - SIP信令客户端
   - 基于eXosip2实现
   - 处理注册、心跳、Invite、目录查询等

4. **GB28181ConfigAPI** (`src/api/include/gb28181_config_api.h`)
   - RESTful API接口
   - 配置管理端点

## API接口

### 1. 获取GB28181配置

**请求:**
```http
GET /api/config/gb28181
```

**响应示例:**
```json
{
  "success": true,
  "enabled": false,
  "sip_server_ip": "",
  "sip_server_port": 5060,
  "sip_server_id": "",
  "sip_server_domain": "",
  "device_id": "",
  "device_password": "",
  "device_name": "",
  "manufacturer": "",
  "model": "",
  "local_sip_port": 5061,
  "rtp_port_start": 30000,
  "rtp_port_end": 30100,
  "heartbeat_interval": 60,
  "heartbeat_count": 3,
  "register_expires": 3600,
  "stream_mode": "PS",
  "max_channels": 32
}
```

### 2. 更新GB28181配置

**请求:**
```http
PUT /api/config/gb28181
Content-Type: application/json

{
  "enabled": true,
  "sip_server_ip": "192.168.1.100",
  "sip_server_port": 5060,
  "sip_server_id": "34020000002000000001",
  "sip_server_domain": "3402000000",
  "device_id": "34020000001320000001",
  "device_password": "12345678",
  "device_name": "视觉分析设备",
  "manufacturer": "YourCompany",
  "model": "VA-2024",
  "local_sip_port": 5061,
  "rtp_port_start": 30000,
  "rtp_port_end": 30100,
  "heartbeat_interval": 60,
  "heartbeat_count": 3,
  "register_expires": 3600,
  "stream_mode": "PS",
  "max_channels": 32
}
```

**响应:**
```json
{
  "success": true
}
```

## 配置参数说明

### SIP配置
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `sip_server_ip` | SIP服务器IP地址 | - |
| `sip_server_port` | SIP服务器端口 | 5060 |
| `sip_server_id` | SIP服务器ID（20位国标编码） | - |
| `sip_server_domain` | SIP服务器域 | - |
| `local_sip_port` | 本地SIP端口 | 5061 |

### 设备配置
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `device_id` | 设备ID（20位国标编码） | - |
| `device_password` | 设备密码 | - |
| `device_name` | 设备名称 | - |
| `manufacturer` | 设备厂商 | - |
| `model` | 设备型号 | - |

### 媒体配置
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `rtp_port_start` | RTP端口起始 | 30000 |
| `rtp_port_end` | RTP端口结束 | 30100 |
| `stream_mode` | 流模式（PS或H264） | PS |

### 心跳配置
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `heartbeat_interval` | 心跳间隔（秒） | 60 |
| `heartbeat_count` | 心跳超时次数 | 3 |
| `register_expires` | 注册有效期（秒） | 3600 |

## GB28181编码规则

### 设备ID编码规则（20位）
```
中心编码(8位) + 行业编码(2位) + 类型编码(3位) + 网络标识(1位) + 序号(6位)
示例: 34020000001320000001
```

### 通道编码生成
```cpp
// 通道编码 = 设备ID前10位 + 通道类型码(131/132) + 通道序号(4位) + 设备ID后3位
// 131 - 前端设备通道
// 132 - IPC通道
// 示例: 34020000001320001001 (通道1)
```

## 使用流程

### 1. 安装依赖

#### Ubuntu/Debian系统
```bash
sudo apt-get install libexosip2-dev libosip2-dev
```

#### 通过vcpkg（推荐）
依赖已添加到 `vcpkg.json`，构建时会自动安装。

### 2. 配置GB28181参数

通过API配置GB28181参数：

```bash
curl -X PUT http://localhost:8080/api/config/gb28181 \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "sip_server_ip": "192.168.1.100",
    "sip_server_port": 5060,
    "sip_server_id": "34020000002000000001",
    "sip_server_domain": "3402000000",
    "device_id": "34020000001320000001",
    "device_password": "12345678",
    "device_name": "视觉分析设备",
    "manufacturer": "YourCompany",
    "model": "VA-2024",
    "local_sip_port": 5061,
    "heartbeat_interval": 60,
    "register_expires": 3600,
    "stream_mode": "PS"
  }'
```

### 3. 启动服务

启动服务后，系统会自动：
- 初始化SIP客户端
- 向SIP服务器注册
- 开始发送心跳保活
- 监听并响应平台的各种请求

```bash
./bin/detector_service
```

### 4. 启动通道分析

当通道启用（`enabled=true`）时，系统会：
- 初始化GB28181通道信息
- 生成通道编码（基于设备ID）
- 等待上级平台的推流请求

### 5. 上级平台操作

在上级平台（如GB28181视频监控平台）进行以下操作：

1. **查看设备列表**：平台会看到已注册的设备
2. **查询通道目录**：平台查询设备通道列表，系统自动响应
3. **点播视频**：平台发送Invite请求点播通道视频
4. **接收视频流**：系统自动启动RTP推流，平台接收并显示视频
5. **停止点播**：平台发送Bye请求，系统停止推流

### 完整工作流程

```
1. 服务启动 → SIP注册成功
2. 定期心跳 → 保持在线状态
3. 平台查询目录 → 响应通道列表
4. 平台点播通道1 → 收到Invite请求
5. 自动启动推流 → 发送200 OK + RTP流
6. 平台接收视频 → 显示检测后的视频
7. 平台停止点播 → 收到Bye请求
8. 自动停止推流 → 释放资源
```

## 工作原理

```
┌─────────────────┐         ┌──────────────────────────────┐         ┌─────────────────┐
│   视频源(RTSP)  │────────>│    视觉分析服务               │<───────>│   GB28181平台   │
│                 │  RTSP   │  - YOLO检测                  │  SIP   │                 │
│                 │  拉流   │  - 绘制检测框                │  信令  │  - SIP服务器    │
└─────────────────┘         │  - GB28181 SIP注册/心跳      │        │  - 媒体网关     │
                            │  - 响应Invite/Catalog        │        │                 │
                            │  - RTP推流(PS/H.264)         │───────>│                 │
                            └──────────────────────────────┘  RTP   └─────────────────┘
                                                              媒体流
```

### 信令交互流程

1. **注册阶段**：设备启动后自动向SIP服务器注册
2. **心跳保活**：定期发送心跳消息，保持在线状态
3. **目录查询**：平台查询设备通道列表，设备自动响应
4. **点播请求**：平台发送Invite请求，设备响应200 OK并启动推流
5. **媒体传输**：RTP推流到平台指定的IP和端口
6. **停止推流**：平台发送Bye请求，设备停止推流

## 当前实现状态

### ✅ 已实现功能
- GB28181配置管理（API + 数据库）
- RTP推流器（基于FFmpeg）
- 通道编码自动生成
- 推流数据封装（PS/H.264）
- 与视觉分析流程集成
- **SIP注册/心跳**（基于eXosip2）
- **SIP信令处理**（Invite/Bye/Message）
- **目录查询响应**
- **设备信息查询响应**
- **设备状态查询响应**

### 🚧 待实现功能
- PTZ控制
- 录像回放
- 报警订阅与推送

## 技术实现

### SIP协议栈
本实现使用 **eXosip2** 和 **osip2** 库来处理SIP信令：
- **eXosip2**：高级SIP会话管理库
- **osip2**：底层SIP消息解析库

### ⚠️ 依赖安装（必需）

**重要**：eXosip2 和 osip2 库不在 vcpkg 中，必须通过系统包管理器安装！

#### Ubuntu/Debian（推荐）
```bash
sudo apt-get update
sudo apt-get install -y libexosip2-dev libosip2-dev

# 验证安装
pkg-config --modversion eXosip2
pkg-config --modversion osip2
```

#### 其他Linux发行版
```bash
# CentOS/RHEL
sudo yum install libeXosip2-devel libosip2-devel

# Fedora
sudo dnf install libeXosip2-devel libosip2-devel

# Arch Linux
sudo pacman -S libexosip2 libosip2
```

**注意**：如果未安装这些库，GB28181 SIP功能将不可用，但不会阻止项目编译。

### 自动功能
- 设备启动时自动注册到SIP服务器
- 自动发送心跳保活
- 自动响应平台的各种查询请求
- 收到Invite时自动启动对应通道的推流
- 收到Bye时自动停止推流

## 测试

### 测试RTP推流
```bash
# 使用ffplay接收RTP流（测试用）
ffplay -protocol_whitelist rtp,udp -i test.sdp

# test.sdp内容示例：
# v=0
# o=- 0 0 IN IP4 127.0.0.1
# s=Stream
# c=IN IP4 127.0.0.1
# t=0 0
# m=video 30000 RTP/AVP 96
# a=rtpmap:96 H264/90000
```

## 日志说明

系统会输出以下GB28181相关日志：
```
GB28181: 初始化推流 rtp://192.168.1.100:30000 (格式: PS, 尺寸: 1920x1080, 帧率: 25)
GB28181: 推流初始化成功
StreamManager: 通道 1 GB28181已启用，通道编码: 34020000001320001001
通道 1 GB28181推流失败  // 推流出错时
GB28181: 推流已关闭
```

## 故障排查

### 问题1: 推流初始化失败
- 检查目标IP和端口是否正确
- 确认网络连接正常
- 检查FFmpeg是否正确安装

### 问题2: 推流数据无法接收
- 确认RTP端口范围配置正确
- 检查防火墙设置
- 验证流格式（PS/H.264）是否匹配

### 问题3: 通道编码错误
- 检查device_id是否为有效的20位国标编码
- 确认通道ID在有效范围内

## 相关标准

- GB/T 28181-2016: 公共安全视频监控联网系统信息传输、交换、控制技术要求
- SIP: RFC 3261
- RTP: RFC 3550
- PS流: GB/T 28181附录

## 技术支持

如有问题，请查看：
- 系统日志: 查看详细的GB28181推流日志
- FFmpeg文档: https://ffmpeg.org/documentation.html
- GB28181标准: 参考国标文档

