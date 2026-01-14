#!/bin/bash

# GB28181 推流测试脚本
# 用于检查推流是否成功

set -e

echo "=========================================="
echo "GB28181 推流测试"
echo "=========================================="

# 检查 SRS 容器是否运行
echo "1. 检查 SRS 容器状态..."
if ! docker ps | grep -q gb28181-srs; then
    echo "❌ SRS 容器未运行"
    echo "   请先启动: docker-compose -f docker-compose-gb28181-test.yml up -d"
    exit 1
else
    echo "✓ SRS 容器正在运行"
fi

# 检查 API 是否可访问
echo ""
echo "2. 检查 SRS API..."
API_URL="http://localhost:1986/api/v1/summaries"
if ! curl -s "$API_URL" > /dev/null; then
    echo "❌ SRS API 不可访问"
    exit 1
else
    echo "✓ SRS API 可访问"
fi

# 获取流列表
echo ""
echo "3. 检查当前活跃的流..."
STREAMS_URL="http://localhost:1986/api/v1/streams/"
STREAMS_RESPONSE=$(curl -s "$STREAMS_URL")
STREAMS_COUNT=$(echo "$STREAMS_RESPONSE" | python3 -c "import sys, json; data=json.load(sys.stdin); print(len(data.get('streams', [])))" 2>/dev/null || echo "0")

if [ "$STREAMS_COUNT" -gt 0 ]; then
    echo "✓ 发现 $STREAMS_COUNT 个活跃的流："
    echo ""
    echo "$STREAMS_RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$STREAMS_RESPONSE"
    echo ""
    
    # 提取流名称
    STREAM_NAMES=$(echo "$STREAMS_RESPONSE" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    streams = data.get('streams', [])
    for stream in streams:
        name = stream.get('name', '')
        app = stream.get('app', '')
        if name:
            print(f'{app}/{name}')
except:
    pass
" 2>/dev/null)
    
    if [ -n "$STREAM_NAMES" ]; then
        echo "=========================================="
        echo "播放地址"
        echo "=========================================="
        while IFS= read -r stream_path; do
            if [ -n "$stream_path" ]; then
                echo ""
                echo "流: $stream_path"
                echo "  HTTP-FLV: http://localhost:9090/$stream_path.flv"
                echo "  HLS:      http://localhost:9090/$stream_path.m3u8"
                echo "  RTMP:     rtmp://localhost:1936/$stream_path"
                echo ""
                echo "  测试播放:"
                echo "    VLC: 打开网络流 -> $stream_path.flv"
                echo "    浏览器: http://localhost:9090/player.html?stream=$stream_path"
            fi
        done <<< "$STREAM_NAMES"
    fi
else
    echo "⚠ 当前没有活跃的流"
    echo ""
    echo "提示："
    echo "  1. 确保设备已成功注册到 SRS"
    echo "  2. 确保设备已发送 INVITE 请求并开始推流"
    echo "  3. 查看 SRS 日志: docker logs -f gb28181-srs"
    echo "  4. 流名称通常是设备ID或通道ID（20位数字）"
fi

# 检查客户端连接
echo ""
echo "4. 检查客户端连接..."
CLIENTS_URL="http://localhost:1986/api/v1/clients/"
CLIENTS_RESPONSE=$(curl -s "$CLIENTS_URL")
CLIENTS_COUNT=$(echo "$CLIENTS_RESPONSE" | python3 -c "import sys, json; data=json.load(sys.stdin); print(len(data.get('clients', [])))" 2>/dev/null || echo "0")

if [ "$CLIENTS_COUNT" -gt 0 ]; then
    echo "✓ 发现 $CLIENTS_COUNT 个客户端连接"
    echo "$CLIENTS_RESPONSE" | python3 -m json.tool 2>/dev/null | head -50
else
    echo "⚠ 当前没有客户端连接"
fi

# 实时监控
echo ""
echo "=========================================="
echo "实时监控（按 Ctrl+C 退出）"
echo "=========================================="
echo "正在监控流状态，每 5 秒刷新一次..."
echo ""

while true; do
    STREAMS_RESPONSE=$(curl -s "$STREAMS_URL")
    STREAMS_COUNT=$(echo "$STREAMS_RESPONSE" | python3 -c "import sys, json; data=json.load(sys.stdin); print(len(data.get('streams', [])))" 2>/dev/null || echo "0")
    
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    if [ "$STREAMS_COUNT" -gt 0 ]; then
        echo "[$TIMESTAMP] ✓ 活跃流数量: $STREAMS_COUNT"
        # 显示流名称
        echo "$STREAMS_RESPONSE" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    streams = data.get('streams', [])
    for stream in streams:
        name = stream.get('name', '')
        app = stream.get('app', '')
        video = stream.get('video', {})
        fps = video.get('fps', 0)
        kbps = video.get('kbps', 0)
        if name:
            print(f'    - {app}/{name} (FPS: {fps}, 码率: {kbps} kbps)')
except:
    pass
" 2>/dev/null
    else
        echo "[$TIMESTAMP] ⚠ 没有活跃的流"
    fi
    sleep 5
done

