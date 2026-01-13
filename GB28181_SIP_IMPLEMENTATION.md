# GB28181 SIP信令实现说明

## 概述

本文档说明GB28181 SIP信令的完整实现，包括设备注册、心跳保活、目录查询、视频点播等功能。

## 新增文件

### 1. SIP客户端头文件
- **文件**: `src/stream/include/gb28181_sip_client.h`
- **功能**: GB28181 SIP客户端类定义

### 2. SIP客户端实现
- **文件**: `src/stream/gb28181_sip_client.cpp`
- **功能**: 完整的SIP信令处理实现

## 核心功能

### 1. SIP注册 (Register)
- 设备启动时自动向SIP服务器注册
- 支持认证（Digest Authentication）
- 定期刷新注册（根据 `register_expires` 配置）

### 2. 心跳保活 (Keepalive)
- 定期发送心跳消息（根据 `heartbeat_interval` 配置）
- 保持设备在线状态
- 心跳格式符合GB28181标准

### 3. 目录查询响应 (Catalog)
- 自动响应平台的目录查询请求
- 返回所有通道的列表和信息
- 通道编码符合GB28181标准（20位国标编码）

### 4. 设备信息查询 (DeviceInfo)
- 响应设备基本信息查询
- 包含厂商、型号、固件版本等

### 5. 设备状态查询 (DeviceStatus)
- 响应设备状态查询
- 返回设备在线状态

### 6. 视频点播 (Invite)
- 接收平台的Invite请求
- 解析SDP获取媒体信息（IP、端口）
- 自动启动对应通道的RTP推流
- 发送200 OK响应和SDP

### 7. 停止点播 (Bye)
- 接收平台的Bye请求
- 自动停止对应通道的推流
- 释放资源

## 技术实现

### 依赖库
- **eXosip2**: 高级SIP会话管理库
- **osip2**: 底层SIP消息解析库

### 线程模型
- **事件处理线程**: 处理SIP事件（Invite、Bye、Message等）
- **心跳线程**: 定期发送心跳消息

### 集成方式
- 集成到 `StreamManager` 中
- 启动时自动初始化SIP客户端
- 通过回调机制触发推流启动/停止

## 配置说明

### 必需配置项
```json
{
  "enabled": true,
  "sip_server_ip": "SIP服务器IP",
  "sip_server_port": 5060,
  "sip_server_id": "20位国标编码",
  "sip_server_domain": "SIP域（通常是编码前10位）",
  "device_id": "20位国标设备编码",
  "device_password": "设备密码",
  "local_sip_port": 5061
}
```

### 可选配置项
```json
{
  "device_name": "设备名称",
  "manufacturer": "厂商名称",
  "model": "设备型号",
  "heartbeat_interval": 60,
  "heartbeat_count": 3,
  "register_expires": 3600,
  "stream_mode": "PS",
  "max_channels": 32,
  "rtp_port_start": 30000,
  "rtp_port_end": 30100
}
```

## 通道编码规则

GB28181通道编码采用20位国标编码：

```
设备ID前10位 + 类型码(131) + 通道序号(4位) + 设备ID后3位
```

**示例**:
- 设备ID: `34020000001320000001`
- 通道1编码: `34020000001310001001`
- 通道2编码: `34020000001310002001`

## SIP消息示例

### 注册消息 (REGISTER)
```
REGISTER sip:192.168.1.100:5060 SIP/2.0
From: <sip:34020000001320000001@3402000000>
To: <sip:34020000002000000001@3402000000>
Contact: <sip:34020000001320000001@127.0.0.1:5061>
Expires: 3600
```

### 心跳消息 (MESSAGE)
```xml
<?xml version="1.0" encoding="GB2312"?>
<Notify>
  <CmdType>Keepalive</CmdType>
  <SN>12345</SN>
  <DeviceID>34020000001320000001</DeviceID>
  <Status>OK</Status>
</Notify>
```

### 目录响应 (Catalog Response)
```xml
<?xml version="1.0" encoding="GB2312"?>
<Response>
  <CmdType>Catalog</CmdType>
  <SN>12345</SN>
  <DeviceID>34020000001320000001</DeviceID>
  <SumNum>32</SumNum>
  <DeviceList Num="32">
    <Item>
      <DeviceID>34020000001310001001</DeviceID>
      <Name>视觉分析设备-通道1</Name>
      <Manufacturer>YourCompany</Manufacturer>
      <Model>VA-2024</Model>
      <Status>ON</Status>
      ...
    </Item>
    ...
  </DeviceList>
</Response>
```

## 工作流程

### 启动流程
1. 服务启动
2. 读取GB28181配置
3. 如果 `enabled=true`，初始化SIP客户端
4. 监听本地SIP端口（默认5061）
5. 向SIP服务器发送REGISTER
6. 开始心跳线程和事件处理线程

### 点播流程
1. 平台发送INVITE请求到通道编码
2. SIP客户端接收并解析INVITE
3. 提取目标IP、端口、通道ID
4. 触发 `handleGB28181Invite` 回调
5. 初始化 `GB28181Streamer`
6. 发送180 Ringing
7. 发送200 OK + SDP
8. 开始RTP推流

### 停止流程
1. 平台发送BYE请求
2. SIP客户端接收并解析BYE
3. 触发 `handleGB28181Bye` 回调
4. 停止 `GB28181Streamer`
5. 释放资源

## 调试建议

### 日志输出
系统会输出详细的日志信息：
```
GB28181 SIP: 初始化成功，监听端口 5061
GB28181 SIP: 已发送注册请求到 sip:192.168.1.100:5060
GB28181 SIP: 注册成功
GB28181 SIP: 收到Invite请求
GB28181: Invite处理完成，通道=34020000001310001001, 目标=192.168.1.100:30000, SSRC=1310001001
GB28181 SIP: 已发送200 OK
GB28181: 通道 1 推流已启动
```

### 常见问题

#### 1. 注册失败
- 检查SIP服务器IP和端口是否正确
- 检查设备ID和密码是否正确
- 检查网络连接是否正常
- 检查防火墙是否放行SIP端口

#### 2. 无法接收Invite
- 确认设备已成功注册
- 确认通道编码格式正确
- 检查SIP端口是否正确监听

#### 3. 推流失败
- 检查RTP端口是否被占用
- 确认目标IP和端口正确
- 检查网络是否可达
- 查看FFmpeg编码日志

## 测试建议

### 1. 使用GB28181测试工具
- 推荐使用 **WVP-PRO** 或 **ZLMediaKit** 作为测试平台
- 可以在本地搭建测试环境

### 2. 使用WireShark抓包
- 抓取SIP信令（UDP 5060/5061）
- 抓取RTP媒体流（UDP 30000-30100）
- 分析信令交互是否正常

### 3. 测试步骤
1. 配置GB28181参数
2. 启动服务，查看注册日志
3. 在平台查看设备是否在线
4. 查询设备目录，确认通道列表正确
5. 点播通道视频，确认推流正常
6. 停止点播，确认资源释放

## 性能优化

### 1. 心跳频率
- 根据网络状况调整 `heartbeat_interval`
- 稳定网络可设置为60-120秒
- 不稳定网络建议30-60秒

### 2. RTP端口范围
- 根据通道数量设置合适的端口范围
- 避免端口冲突
- 建议预留一些备用端口

### 3. 推流参数
- 根据网络带宽选择合适的码率
- 根据平台要求选择PS或H.264格式
- 调整GOP大小以平衡延迟和质量

## 安全建议

### 1. SIP认证
- 使用强密码
- 定期更换设备密码
- 启用SIP认证（Digest Authentication）

### 2. 网络隔离
- 将GB28181服务部署在独立VLAN
- 使用防火墙限制访问
- 只开放必要的端口

### 3. 日志审计
- 记录所有SIP信令交互
- 监控异常注册尝试
- 定期检查日志

## 未来扩展

### 可能的增强功能
1. **PTZ控制**: 响应云台控制指令
2. **录像回放**: 支持历史录像查询和回放
3. **报警推送**: 检测到目标时主动推送报警
4. **级联支持**: 支持多级级联组网
5. **TLS加密**: 支持SIP over TLS

## 参考资料

- GB/T 28181-2016: 公共安全视频监控联网系统信息传输、交换、控制技术要求
- RFC 3261: SIP协议规范
- RFC 3550: RTP协议规范
- eXosip2文档: http://www.antisip.com/doc/exosip2/
- osip2文档: http://www.gnu.org/software/osip/

## 联系支持

如有问题或建议，请查看项目文档或联系开发团队。

