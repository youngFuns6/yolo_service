# WebSocket 连接问题修复

## 当前状态

- ✅ 代码已修复并重新编译
- ⚠️ 需要重启服务器以使用新版本
- ⚠️ 连接测试失败（可能是旧版本服务器仍在运行）

## 已添加的调试信息

代码中已添加详细的调试输出，包括：
1. 服务器线程启动日志
2. poll 检测到新连接的日志
3. accept 成功/失败的日志
4. 连接处理开始的日志
5. recv 调用的详细日志（前3次）
6. 请求头读取过程的日志
7. WebSocket 升级检测的日志
8. 握手响应发送的日志

## 测试步骤

### 1. 停止旧服务器

```bash
# 查找并停止旧进程
pkill detector_service
# 或
killall detector_service
```

### 2. 启动新服务器

```bash
cd /Users/youngfuns/Documents/Project/yolo_service
./build/macos-arm64-Release/bin/detector_service
```

**重要**：启动后应该能看到：
- `WebSocket 服务器已启动在端口 9091`
- `WebSocket: 服务器线程已启动，监听 client_fd: X`

### 3. 在另一个终端运行测试

```bash
./test_ws_quick.sh
```

### 4. 查看服务器日志

当有连接时，应该能看到类似这样的输出：
```
WebSocket: poll 检测到新连接请求
WebSocket: accept 成功，client_fd: X
WebSocket: 接受新连接，client_fd: X, IP: 127.0.0.1, Port: XXXX
WebSocket: 开始处理连接，client_fd: X
WebSocket: 开始读取请求头...
WebSocket: 调用 recv (尝试 1)，已读取: 0 字节
WebSocket: recv 返回: XXX
...
```

## 如果仍然失败

请检查服务器日志中的：
1. 是否有 "WebSocket: poll 检测到新连接请求" - 如果没有，说明 poll 没有检测到连接
2. 是否有 "WebSocket: accept 成功" - 如果没有，说明 accept 失败
3. 是否有 "WebSocket: recv 返回" - 如果没有，说明 recv 阻塞或失败
4. recv 返回的值和 errno - 这将告诉我们具体的问题

## 常见问题

### 问题 1: 没有看到任何日志
- **原因**：服务器可能还在运行旧版本
- **解决**：确保完全停止旧服务器并启动新版本

### 问题 2: 看到 accept 成功但没有 recv 日志
- **原因**：线程可能没有启动，或 recv 阻塞
- **解决**：检查线程是否正确启动，检查 socket 模式

### 问题 3: recv 返回 -1 且 errno != EAGAIN
- **原因**：socket 错误
- **解决**：查看具体的 errno 值，检查 socket 状态
