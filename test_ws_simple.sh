#!/bin/bash
# 简单的 WebSocket 测试脚本

echo "测试 WebSocket 连接..."
echo ""

# 测试通道 WebSocket
echo "1. 测试通道数据 WebSocket (ws://localhost:9091/ws/channel)"
echo "   使用 curl 发送 WebSocket 升级请求..."
curl -v \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
  -H "Sec-WebSocket-Version: 13" \
  http://localhost:9091/ws/channel 2>&1 | grep -E "(HTTP|Upgrade|Connection|Sec-WebSocket)" | head -10

echo ""
echo "2. 如果看到 'HTTP/1.1 101 Switching Protocols'，说明 WebSocket 握手成功"
echo ""

# 提示使用 Python 测试脚本
echo "3. 使用 Python 测试脚本进行完整测试:"
echo "   python3 test_websocket.py"
echo ""
