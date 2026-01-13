#!/bin/bash

# GB28181 客户端注册测试脚本
# 用于测试 GB28181 客户端是否能成功注册到 SRS 服务器

set -e

echo "=========================================="
echo "GB28181 客户端注册测试"
echo "=========================================="

# 检查 SRS 容器是否运行
echo "1. 检查 SRS 容器状态..."
if ! docker ps | grep -q gb28181-srs; then
    echo "❌ SRS 容器未运行，正在启动..."
    docker-compose -f docker-compose-gb28181-test.yml up -d
    echo "等待 SRS 启动..."
    sleep 5
else
    echo "✓ SRS 容器正在运行"
fi

# 检查 SRS API 是否可访问
echo ""
echo "2. 检查 SRS API 连接..."
API_URL="http://localhost:1986/api/v1/summaries"
if curl -s "$API_URL" > /dev/null; then
    echo "✓ SRS API 可访问"
    echo "   服务器摘要: $API_URL"
else
    echo "❌ SRS API 不可访问，请检查容器状态"
    exit 1
fi

# 检查 SIP 端口
echo ""
echo "3. 检查 SIP 端口..."
if nc -z localhost 5062 2>/dev/null; then
    echo "✓ SIP 端口 5062 可访问"
else
    echo "⚠ SIP 端口 5062 不可访问（可能正常，SIP 使用 UDP）"
fi

# 显示当前注册的设备（注意：SRS GB28181 API 路径可能不同）
echo ""
echo "4. 查看已注册的 GB28181 设备..."
# 尝试多个可能的 API 路径
DEVICES_URL="http://localhost:1986/api/v1/gb28181/devices"
DEVICES=$(curl -s "$DEVICES_URL" 2>/dev/null)
if echo "$DEVICES" | grep -q "code.*0" 2>/dev/null; then
    echo "✓ GB28181 API 响应："
    echo "$DEVICES" | python3 -m json.tool 2>/dev/null || echo "$DEVICES"
else
    echo "⚠ GB28181 设备 API 路径可能不同，请查看 SRS 日志确认设备注册状态"
    echo "   提示：设备注册后会在 SRS 日志中显示"
fi

# 显示流列表
echo ""
echo "5. 查看当前流..."
STREAMS_URL="http://localhost:1986/api/v1/streams/"
STREAMS=$(curl -s "$STREAMS_URL" 2>/dev/null || echo "[]")
if [ "$STREAMS" != "[]" ] && [ -n "$STREAMS" ]; then
    echo "✓ 当前流："
    echo "$STREAMS" | python3 -m json.tool 2>/dev/null || echo "$STREAMS"
else
    echo "⚠ 当前没有活跃的流"
fi

# 显示配置建议
echo ""
echo "=========================================="
echo "客户端配置建议"
echo "=========================================="
echo "SIP 服务器 IP: localhost (或容器IP)"
echo "SIP 服务器端口: 5062"
echo "SIP 服务器 ID: 34020000002000000001"
echo "SIP 服务器域: 3402000000"
echo ""
echo "设备配置示例："
echo "  设备 ID: 34020000001320000001 (20位)"
echo "  设备密码: (可选，当前配置为不认证)"
echo "  本地 SIP 端口: 5061"
echo ""

# 监控日志
echo "=========================================="
echo "实时监控 SRS 日志（按 Ctrl+C 退出）"
echo "=========================================="
echo "正在查看 SRS 日志..."
docker logs -f gb28181-srs
