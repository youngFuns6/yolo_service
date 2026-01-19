#!/usr/bin/env python3
"""
WebSocket 连接测试脚本
用于测试 detector_service 的 WebSocket 功能
"""

import asyncio
import websockets
import json
import sys
import signal

# WebSocket 服务器地址
WS_CHANNEL_URL = "ws://localhost:9091/ws/channel"
WS_ALERT_URL = "ws://localhost:9091/ws/alert"

def signal_handler(sig, frame):
    print("\n\n正在关闭连接...")
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

async def test_channel_websocket():
    """测试通道数据 WebSocket 连接"""
    print("=" * 60)
    print("测试通道数据 WebSocket 连接")
    print("=" * 60)
    print(f"连接到: {WS_CHANNEL_URL}")
    
    try:
        async with websockets.connect(WS_CHANNEL_URL) as websocket:
            print("✓ WebSocket 连接成功！")
            print("\n等待服务器确认消息...")
            
            # 等待连接确认
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                data = json.loads(message)
                print(f"收到消息: {json.dumps(data, indent=2, ensure_ascii=False)}")
            except asyncio.TimeoutError:
                print("未收到确认消息（可能正常）")
            
            # 测试订阅通道
            print("\n发送订阅请求（通道 ID: 1）...")
            subscribe_msg = {
                "action": "subscribe",
                "channel_id": 1
            }
            await websocket.send(json.dumps(subscribe_msg))
            print(f"已发送: {json.dumps(subscribe_msg, ensure_ascii=False)}")
            
            # 等待订阅确认
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                data = json.loads(message)
                print(f"收到订阅确认: {json.dumps(data, indent=2, ensure_ascii=False)}")
            except asyncio.TimeoutError:
                print("未收到订阅确认（可能服务器没有响应）")
            
            # 等待接收数据（最多等待 10 秒）
            print("\n等待接收数据（最多 10 秒）...")
            try:
                for i in range(10):
                    message = await asyncio.wait_for(websocket.recv(), timeout=1.0)
                    data = json.loads(message)
                    msg_type = data.get("type", "unknown")
                    if msg_type == "frame":
                        print(f"收到帧数据: 通道 {data.get('channel_id')}, 时间戳: {data.get('timestamp')}")
                        print(f"  图片大小: {len(data.get('image_base64', ''))} 字节")
                    elif msg_type == "subscription_confirmed":
                        print(f"订阅确认: {json.dumps(data, indent=2, ensure_ascii=False)}")
                    else:
                        print(f"收到消息: {json.dumps(data, indent=2, ensure_ascii=False)}")
            except asyncio.TimeoutError:
                print("等待超时（没有收到数据）")
            
            print("\n✓ 通道 WebSocket 测试完成")
            
    except websockets.exceptions.InvalidURI:
        print(f"✗ 错误: 无效的 WebSocket URL: {WS_CHANNEL_URL}")
    except ConnectionRefusedError:
        print(f"✗ 错误: 无法连接到服务器 {WS_CHANNEL_URL}")
        print("  请确保 detector_service 正在运行，并且 WebSocket 服务器已启动")
    except Exception as e:
        print(f"✗ 错误: {type(e).__name__}: {e}")

async def test_alert_websocket():
    """测试报警数据 WebSocket 连接"""
    print("\n" + "=" * 60)
    print("测试报警数据 WebSocket 连接")
    print("=" * 60)
    print(f"连接到: {WS_ALERT_URL}")
    
    try:
        async with websockets.connect(WS_ALERT_URL) as websocket:
            print("✓ WebSocket 连接成功！")
            print("\n等待服务器确认消息...")
            
            # 等待连接确认
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                data = json.loads(message)
                print(f"收到消息: {json.dumps(data, indent=2, ensure_ascii=False)}")
            except asyncio.TimeoutError:
                print("未收到确认消息（可能正常）")
            
            # 等待接收报警数据（最多等待 10 秒）
            print("\n等待接收报警数据（最多 10 秒）...")
            try:
                for i in range(10):
                    message = await asyncio.wait_for(websocket.recv(), timeout=1.0)
                    data = json.loads(message)
                    msg_type = data.get("type", "unknown")
                    if msg_type == "alert":
                        print(f"收到报警: 通道 {data.get('channel_id')}, 类型: {data.get('alert_type')}")
                        print(f"  置信度: {data.get('confidence')}, 时间戳: {data.get('timestamp')}")
                    elif msg_type == "alert_subscription_confirmed":
                        print(f"订阅确认: {json.dumps(data, indent=2, ensure_ascii=False)}")
                    else:
                        print(f"收到消息: {json.dumps(data, indent=2, ensure_ascii=False)}")
            except asyncio.TimeoutError:
                print("等待超时（没有收到报警数据）")
            
            print("\n✓ 报警 WebSocket 测试完成")
            
    except websockets.exceptions.InvalidURI:
        print(f"✗ 错误: 无效的 WebSocket URL: {WS_ALERT_URL}")
    except ConnectionRefusedError:
        print(f"✗ 错误: 无法连接到服务器 {WS_ALERT_URL}")
        print("  请确保 detector_service 正在运行，并且 WebSocket 服务器已启动")
    except Exception as e:
        print(f"✗ 错误: {type(e).__name__}: {e}")

async def main():
    """主测试函数"""
    print("\n" + "=" * 60)
    print("WebSocket 连接测试")
    print("=" * 60)
    print(f"通道数据端点: {WS_CHANNEL_URL}")
    print(f"报警数据端点: {WS_ALERT_URL}")
    print("\n提示: 按 Ctrl+C 退出测试\n")
    
    # 测试通道 WebSocket
    await test_channel_websocket()
    
    # 测试报警 WebSocket
    await test_alert_websocket()
    
    print("\n" + "=" * 60)
    print("所有测试完成")
    print("=" * 60)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n测试已取消")
        sys.exit(0)
