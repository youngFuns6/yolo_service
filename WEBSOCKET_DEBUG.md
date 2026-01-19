# WebSocket 连接调试指南

## 当前状态

- ✅ WebSocket 服务器代码已实现
- ✅ 编译成功
- ⚠️ 连接测试失败（服务器未响应）

## 问题诊断

### 可能的原因

1. **服务器未重启**：当前运行的可能是旧版本的服务器
2. **Socket 读取问题**：非阻塞/阻塞模式切换可能有问题
3. **请求头解析问题**：可能没有正确识别 WebSocket 升级请求

## 调试步骤

### 1. 重启服务器

```bash
# 停止当前服务器（如果有）
pkill detector_service

# 启动新版本
./build/macos-arm64-Release/bin/detector_service
```

### 2. 查看服务器日志

启动服务器后，应该能看到：
- `WebSocket 服务器已启动在端口 9091`
- 当有连接时，应该看到：
  - `WebSocket: 接受新连接，client_fd: X, IP: ..., Port: ...`
  - `WebSocket: 开始处理连接，client_fd: X`
  - `WebSocket: 开始读取请求头...`
  - `WebSocket: 调用 recv，已读取: 0 字节`
  - `WebSocket: recv 返回: X`

### 3. 运行测试

```bash
# 快速测试
./test_ws_quick.sh

# 或使用 Python 测试
python3 test_websocket.py
```

## 已添加的调试信息

代码中已添加详细的调试输出：
- 连接接受日志
- 请求头读取过程
- WebSocket 升级检测
- 握手响应发送

## 如果仍然失败

请检查：
1. 服务器日志中的错误信息
2. 是否有线程崩溃
3. socket 文件描述符是否正确

## 下一步

如果问题持续，可能需要：
1. 检查线程是否正确启动
2. 验证 socket 选项设置
3. 检查是否有异常被捕获但未输出
