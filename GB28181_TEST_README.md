# GB28181 客户端注册测试指南

本指南说明如何使用 SRS 服务器测试 GB28181 客户端注册功能。

## 快速开始

### 1. 启动 SRS 服务器

```bash
docker-compose -f docker-compose-gb28181-test.yml up -d
```

### 2. 检查服务器状态

```bash
# 查看容器状态
docker ps | grep gb28181-srs

# 查看日志
docker logs -f gb28181-srs

# 或使用测试脚本
./test_gb28181_registration.sh
```

### 3. 访问测试页面

打开浏览器访问：http://localhost:8081

## SRS 服务器配置

### SIP 服务器信息

- **SIP 服务器 IP**: `localhost` (或容器实际IP)
- **SIP 服务器端口**: `5062` (映射自容器内的 5060)
- **SIP 服务器 ID**: `34020000002000000001` (20位国标编码)
- **SIP 服务器域**: `3402000000`
- **认证**: 当前配置为不认证 (`sip_auth=0`)
- **RTP 端口范围**: `30100-30200` (映射自容器内的 30000-30100)

### 端口映射

| 服务 | 容器内端口 | 宿主机端口 | 说明 |
|------|-----------|-----------|------|
| SIP | 5060 | 5062 | GB28181 SIP 协议 |
| HTTP API | 1985 | 1986 | SRS API 接口 |
| RTMP | 1935 | 1936 | RTMP 推流 |
| HTTP | 8081 | 8081 | HTTP 播放和测试页面 |
| RTP | 30000-30100 | 30100-30200 | RTP 媒体流 |

## 客户端配置示例

### 配置参数

在您的应用程序中配置 GB28181 客户端时，使用以下参数：

```cpp
GB28181Config config;
config.enabled = true;
config.sip_server_ip = "127.0.0.1";  // 或容器IP
config.sip_server_port = 5062;       // 映射后的端口
config.sip_server_id = "34020000002000000001";
config.sip_server_domain = "3402000000";
config.device_id = "34020000001320000001";  // 20位设备ID
config.device_password = "";  // 当前配置不需要密码
config.device_name = "测试设备";
config.manufacturer = "Test";
config.model = "GB28181-Test";
config.local_sip_port = 5061;
config.rtp_port_start = 30000;
config.rtp_port_end = 30100;
config.heartbeat_interval = 60;
config.register_expires = 3600;
config.stream_mode = "PS";
config.max_channels = 32;
```

### 设备 ID 格式说明

GB28181 设备 ID 为 20 位数字，格式如下：
- 前 8 位：中心编码（区域编码）
- 第 9-10 位：行业编码
- 第 11-13 位：类型编码（131=前端设备通道）
- 第 14 位：网络标识
- 后 6 位：设备编码

示例：
- 服务器 ID: `34020000002000000001`
- 设备 ID: `34020000001320000001`

## 测试步骤

### 1. 启动 SRS 服务器

```bash
docker-compose -f docker-compose-gb28181-test.yml up -d
```

### 2. 验证服务器运行

```bash
# 检查 API 是否可访问
curl http://localhost:1986/api/v1/summaries

# 查看已注册设备
curl http://localhost:1986/api/v1/gb28181/devices
```

### 3. 配置并启动客户端

在您的应用程序中：
1. 设置 GB28181 配置（使用上述参数）
2. 启用 GB28181 功能
3. 启动服务

### 4. 验证注册

```bash
# 使用测试脚本
./test_gb28181_registration.sh

# 或手动检查
curl http://localhost:1986/api/v1/gb28181/devices | python3 -m json.tool
```

### 5. 查看日志

```bash
# SRS 服务器日志
docker logs -f gb28181-srs

# 客户端日志（在您的应用程序中查看）
```

### 6. 测试推流

设备注册成功后，需要发送 INVITE 请求来启动推流。

#### 方法一：使用命令行测试脚本

```bash
./test_gb28181_stream.sh
```

该脚本会：
- 检查 SRS 容器状态
- 列出所有活跃的流
- 显示每个流的播放地址
- 实时监控流状态

#### 方法二：使用 Web 播放器

打开浏览器访问：http://localhost:8081/player.html

播放器功能：
- 自动加载当前活跃的流列表
- 支持 HLS 播放（推荐）
- 显示流的详细信息（FPS、码率等）
- 支持手动输入流名称播放

#### 方法三：使用 VLC 播放器

如果流名称为 `live/34020000001320000001`，可以使用 VLC 播放：

**HTTP-FLV 播放：**
```
打开 VLC -> 媒体 -> 打开网络串流
输入：http://localhost:8081/live/34020000001320000001.flv
```

**HLS 播放：**
```
打开 VLC -> 媒体 -> 打开网络串流
输入：http://localhost:8081/live/34020000001320000001.m3u8
```

**RTMP 播放：**
```
打开 VLC -> 媒体 -> 打开网络串流
输入：rtmp://localhost:1936/live/34020000001320000001
```

#### 流名称格式

SRS GB28181 的流名称通常格式为：
- `live/[设备ID]` - 设备推流
- `live/[通道ID]` - 通道推流

例如：
- `live/34020000001320000001` - 设备推流
- `live/34020000001310000001` - 通道推流

#### 验证推流成功的标志

推流成功的标志：
- ✅ 在流列表中看到对应的流
- ✅ 流的状态为 "publishing"
- ✅ 流的 FPS 和码率大于 0
- ✅ 可以使用播放器正常播放
- ✅ SRS 日志中显示 RTP 数据接收

## API 接口

### 查看服务器摘要

```bash
curl http://localhost:1986/api/v1/summaries
```

### 查看已注册设备

```bash
curl http://localhost:1986/api/v1/gb28181/devices
```

### 查看流列表

```bash
curl http://localhost:1986/api/v1/streams/
```

### 查看客户端列表

```bash
curl http://localhost:1986/api/v1/clients/
```

## 故障排查

### 问题：Docker 镜像拉取失败（403 Forbidden 或其他错误）

**错误信息示例**：
```
Error response from daemon: failed to resolve reference "docker.io/7040210/gb28181:latest": 
unexpected status from HEAD request to https://docker.m.daocloud.io/v2/...: 403 Forbidden
```

**解决方案**：

1. **使用修复脚本（推荐）**：
   ```bash
   ./fix_gb28181_image.sh
   ```
   该脚本会自动尝试多种方式拉取镜像。

2. **手动拉取镜像**：
   ```bash
   # 直接从 Docker Hub 拉取
   docker pull 7040210/gb28181:latest
   
   # 或从阿里云镜像源拉取
   docker pull registry.cn-hangzhou.aliyuncs.com/7040210/gb28181:latest
   docker tag registry.cn-hangzhou.aliyuncs.com/7040210/gb28181:latest 7040210/gb28181:latest
   ```

3. **配置 Docker 镜像加速器**：
   - Windows: Docker Desktop -> Settings -> Docker Engine
   - 添加镜像源配置：
     ```json
     {
       "registry-mirrors": [
         "https://docker.mirrors.ustc.edu.cn",
         "https://hub-mirror.c.163.com",
         "https://mirror.baidubce.com"
       ]
     }
     ```
   - 重启 Docker Desktop

4. **使用备选镜像**：
   编辑 `docker-compose-gb28181-test.yml`，将：
   ```yaml
   image: 7040210/gb28181:latest
   ```
   改为：
   ```yaml
   image: ossrs/srs:4
   ```
   ⚠️ 注意：官方 SRS 镜像可能不包含 GB28181 支持，需要自行编译。

5. **检查网络连接**：
   ```bash
   # 测试网络连接
   ping docker.io
   curl -I https://docker.io
   ```

### 问题：容器无法启动

**解决方案**：
1. 检查端口是否被占用：`lsof -i :5062` (Linux/Mac) 或 `netstat -ano | findstr :5062` (Windows)
2. 检查配置文件是否存在：`ls -la gb28181-test-data/srs/srs.conf`
3. 查看容器日志：`docker logs gb28181-srs`
4. 检查镜像是否存在：`docker images | grep gb28181`

### 问题：客户端无法注册

**检查清单**：
1. ✅ SRS 容器是否运行：`docker ps | grep gb28181-srs`
2. ✅ SIP 端口是否正确：客户端应使用 `5062`（不是 5060）
3. ✅ 设备 ID 格式是否正确：必须是 20 位数字
4. ✅ SIP 服务器 ID 和域是否匹配
5. ✅ 网络连接是否正常：`ping localhost` 或容器IP
6. ✅ 查看 SRS 日志：`docker logs -f gb28181-srs`
7. ✅ 查看客户端日志

### 问题：注册成功但无法推流

**检查清单**：
1. ✅ RTP 端口范围是否正确映射
2. ✅ 防火墙是否阻止 UDP 端口
3. ✅ 查看 SRS 日志中的错误信息
4. ✅ 检查客户端是否正确响应 INVITE 请求

### 问题：设备已注册但没有流

**可能原因**：
1. 设备未发送 INVITE 请求
2. INVITE 请求被拒绝
3. RTP 端口未正确映射
4. 网络连接问题

**解决方案**：
1. 检查客户端是否正确响应 INVITE
2. 查看 SRS 日志中的错误信息
3. 确认 RTP 端口范围是否正确
4. 使用 `tcpdump` 抓包检查 RTP 数据

### 问题：流存在但无法播放

**可能原因**：
1. 流格式不支持
2. 编码格式不兼容
3. 播放器不支持该格式

**解决方案**：
1. 尝试不同的播放地址（FLV、HLS、RTMP）
2. 检查流的编码信息
3. 使用 VLC 等专业播放器

## 调试技巧

### 使用 Wireshark 抓包

```bash
# 抓取 SIP 流量
sudo tcpdump -i lo0 -w sip.pcap port 5062

# 抓取 RTP 流量
sudo tcpdump -i lo0 -w rtp.pcap portrange 30100-30200
```

### 查看详细日志

SRS 日志位置（容器内）：`/usr/local/srs/objs/srs.log`

```bash
# 实时查看日志
docker exec -it gb28181-srs tail -f /usr/local/srs/objs/srs.log
```

## 配置修改

如需修改 SRS 配置：

1. 编辑配置文件：`gb28181-test-data/srs/srs.conf`
2. 重启容器：`docker-compose -f docker-compose-gb28181-test.yml restart`

## 停止服务

```bash
docker-compose -f docker-compose-gb28181-test.yml down
```

## 参考资源

- [SRS GB28181 文档](https://github.com/ossrs/srs/wiki/v4_CN_GB28181)
- [GB28181 标准文档](http://www.gb688.cn/bzgk/gb/newGbInfo?hcno=469659DC56B9B8187671FF0874C3CD1D)

