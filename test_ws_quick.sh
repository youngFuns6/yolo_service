#!/bin/bash
# 快速 WebSocket 连接测试

echo "=========================================="
echo "WebSocket 连接快速测试"
echo "=========================================="
echo ""

# 检查服务器是否运行
if ! lsof -i :9091 > /dev/null 2>&1; then
    echo "✗ WebSocket 服务器未运行（端口 9091 未被占用）"
    echo "  请先启动 detector_service"
    exit 1
fi

echo "✓ WebSocket 服务器正在运行（端口 9091）"
echo ""

# 测试连接
echo "测试 WebSocket 握手..."
python3 << 'EOF'
import socket
import sys

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3)
    s.connect(('localhost', 9091))
    print("✓ 已连接到服务器")
    
    request = b'''GET /ws/channel HTTP/1.1\r
Host: localhost:9091\r
Upgrade: websocket\r
Connection: Upgrade\r
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r
Sec-WebSocket-Version: 13\r
\r
'''
    s.send(request)
    print("✓ 已发送 WebSocket 握手请求")
    
    response = s.recv(4096)
    if response:
        resp_str = response.decode('utf-8', errors='ignore')
        print(f"✓ 收到响应 ({len(response)} 字节)")
        if '101' in resp_str or 'Switching Protocols' in resp_str:
            print("✓✓✓ WebSocket 握手成功！")
            sys.exit(0)
        else:
            print("✗ WebSocket 握手失败")
            print("响应内容:")
            print(resp_str[:500])
            sys.exit(1)
    else:
        print("✗ 未收到响应")
        sys.exit(1)
except socket.timeout:
    print("✗ 超时：没有收到响应")
    print("  请检查服务器日志")
    sys.exit(1)
except Exception as e:
    print(f"✗ 错误: {e}")
    sys.exit(1)
finally:
    s.close()
EOF

exit_code=$?
echo ""
if [ $exit_code -eq 0 ]; then
    echo "=========================================="
    echo "✓ 测试通过！WebSocket 连接正常"
    echo "=========================================="
else
    echo "=========================================="
    echo "✗ 测试失败！请检查："
    echo "  1. 服务器是否已重启（使用新编译的版本）"
    echo "  2. 查看服务器日志输出"
    echo "  3. 检查端口 9091 是否被正确监听"
    echo "=========================================="
fi
