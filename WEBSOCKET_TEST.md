# WebSocket 连接测试指南

## 前提条件

1. **确保服务器正在运行**
   - WebSocket 服务器应该在端口 9091 上运行
   - HTTP 服务器应该在端口 9090 上运行（如果端口被占用，请更改配置）

2. **安装 Python websockets 库**（如果使用 Python 测试脚本）
   ```bash
   pip3 install websockets
   ```

## 测试方法

### 方法 1: 使用 Python 测试脚本（推荐）

```bash
python3 test_websocket.py
```

这个脚本会：
- 测试通道数据 WebSocket 连接 (`ws://localhost:9091/ws/channel`)
- 测试报警数据 WebSocket 连接 (`ws://localhost:9091/ws/alert`)
- 发送订阅请求并等待响应

### 方法 2: 使用 curl 测试握手

```bash
curl -v \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
  -H "Sec-WebSocket-Version: 13" \
  http://localhost:9091/ws/channel
```

如果看到 `HTTP/1.1 101 Switching Protocols`，说明握手成功。

### 方法 3: 使用浏览器控制台

在浏览器中打开开发者工具，运行：

```javascript
// 测试通道 WebSocket
const ws = new WebSocket('ws://localhost:9091/ws/channel');
ws.onopen = () => console.log('连接已建立');
ws.onmessage = (event) => console.log('收到消息:', JSON.parse(event.data));
ws.onerror = (error) => console.error('错误:', error);
ws.onclose = (event) => console.log('连接已关闭:', event.code, event.reason);

// 订阅通道
ws.send(JSON.stringify({action: 'subscribe', channel_id: 1}));
```

## 预期结果

### 成功的情况：
- WebSocket 连接成功建立
- 收到订阅确认消息
- 如果通道有数据，会收到帧数据或报警数据

### 失败的情况：
- 连接被拒绝：检查服务器是否运行
- 握手失败：检查 WebSocket 服务器日志
- 无响应：检查路由配置

## 调试

如果连接失败，检查：

1. **服务器日志**：查看是否有错误信息
2. **端口占用**：`lsof -i :9091`
3. **防火墙**：确保端口未被阻止

## WebSocket 端点

- **通道数据**: `ws://localhost:9091/ws/channel`
- **报警数据**: `ws://localhost:9091/ws/alert`

## 消息格式

### 订阅通道
```json
{
  "action": "subscribe",
  "channel_id": 1
}
```

### 订阅确认
```json
{
  "type": "subscription_confirmed",
  "channel_id": 1
}
```

### 帧数据
```json
{
  "type": "frame",
  "channel_id": 1,
  "image_base64": "...",
  "timestamp": "2024-01-01 12:00:00"
}
```

### 报警数据
```json
{
  "type": "alert",
  "channel_id": 1,
  "alert_type": "...",
  "confidence": 0.95,
  "timestamp": "..."
}
```
