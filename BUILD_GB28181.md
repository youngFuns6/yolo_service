# GB28181 SIP功能构建指南

## 前置要求

### 依赖安装方式

**优先使用 vcpkg 安装**（如果 vcpkg 中有这些包）：
- `libosip2` - 已在 vcpkg.json 中配置
- `exosip2` - 已在 vcpkg.json 中配置（如果 vcpkg 中有此包）

**如果 vcpkg 中没有这些包，需要通过系统包管理器安装：**

#### 方式1：通过 vcpkg（推荐，如果包存在）

vcpkg.json 中已配置 `libosip2` 和 `exosip2`，构建时会自动安装。

如果构建时提示找不到包，请使用方式2。

#### 方式2：通过系统包管理器（备用方案）

**Ubuntu/Debian**
```bash
# 更新包列表
sudo apt-get update

# 安装eXosip2和osip2库
sudo apt-get install -y libexosip2-dev libosip2-dev

# 验证安装
pkg-config --modversion eXosip2
pkg-config --modversion osip2

# 应该看到版本号，例如：
# 5.3.0
# 5.3.0
```

#### 其他Linux发行版
```bash
# CentOS/RHEL
sudo yum install libeXosip2-devel libosip2-devel

# Fedora
sudo dnf install libeXosip2-devel libosip2-devel

# Arch Linux
sudo pacman -S libexosip2 libosip2
```

## 构建步骤

### 1. 确认系统库已安装

**在配置CMake之前，必须先安装系统依赖！**

```bash
# 检查是否已安装
pkg-config --exists eXosip2 && echo "eXosip2 found" || echo "eXosip2 NOT found"
pkg-config --exists osip2 && echo "osip2 found" || echo "osip2 NOT found"

# 如果未安装，执行：
sudo apt-get install -y libexosip2-dev libosip2-dev
```

### 2. 配置CMake

```bash
cd /mnt/d/Project/detector_service

# 创建构建目录
mkdir -p build
cd build

# 配置CMake（使用vcpkg工具链）
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# 或者如果系统已安装所有依赖
cmake ..
```

**注意**：CMake 会自动通过 pkg-config 查找 eXosip2 库。如果找不到，会尝试手动查找。如果都找不到，会显示警告但不会阻止编译（GB28181 SIP功能将不可用）。

### 3. 编译

```bash
# 编译项目
cmake --build . -j$(nproc)

# 或使用make
make -j$(nproc)
```

### 4. 验证构建

检查是否成功链接eXosip2库：

```bash
# 查看依赖库
ldd bin/detector_service | grep exosip
ldd bin/detector_service | grep osip

# 应该看到类似输出：
# libexosip2.so.15 => /usr/lib/x86_64-linux-gnu/libexosip2.so.15
# libosip2.so.15 => /usr/lib/x86_64-linux-gnu/libosip2.so.15
```

## 常见构建问题

### 问题1: 找不到eXosip2库

**错误信息**:
```
Could not find eXosip2
eXosip2 libraries not found, GB28181 SIP signaling will not work
```

**解决方法**:
```bash
# 1. 首先安装系统库（必需）
sudo apt-get update
sudo apt-get install -y libexosip2-dev libosip2-dev

# 2. 验证安装
pkg-config --modversion eXosip2
pkg-config --modversion osip2

# 3. 如果pkg-config找不到，检查路径
export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# 4. 重新配置CMake
cd build
rm -rf *
cmake ..
```

### 问题2: 链接错误

**错误信息**:
```
undefined reference to `eXosip_init'
```

**解决方法**:
检查CMakeLists.txt是否正确链接库：
```cmake
target_link_libraries(stream PUBLIC
    ${EXOSIP2_LIBRARY}
    ${OSIP2_LIBRARY}
    ${OSIPPARSER2_LIBRARY}
)
```

### 问题3: 头文件找不到

**错误信息**:
```
fatal error: eXosip2/eXosip.h: No such file or directory
```

**解决方法**:
```bash
# 确认头文件路径
find /usr -name "eXosip.h" 2>/dev/null

# 添加到CMakeLists.txt
target_include_directories(stream PUBLIC /usr/include)
```

## 运行测试

### 1. 启动服务

```bash
cd build
./bin/detector_service
```

### 2. 配置GB28181

```bash
curl -X PUT http://localhost:8080/api/config/gb28181 \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "sip_server_ip": "192.168.1.100",
    "sip_server_port": 5060,
    "sip_server_id": "34020000002000000001",
    "sip_server_domain": "3402000000",
    "device_id": "34020000001320000001",
    "device_password": "12345678",
    "device_name": "测试设备",
    "manufacturer": "TestCompany",
    "model": "VA-TEST",
    "local_sip_port": 5061
  }'
```

### 3. 重启服务以应用配置

```bash
# 按Ctrl+C停止服务
# 重新启动
./bin/detector_service
```

### 4. 查看日志

观察日志输出，应该看到：
```
GB28181 SIP: 初始化成功，监听端口 5061
GB28181 SIP: 已发送注册请求到 sip:192.168.1.100:5060
GB28181 SIP: 客户端启动成功
```

如果注册成功，会显示：
```
GB28181 SIP: 注册成功
```

## 调试模式

### 启用详细日志

修改 `gb28181_sip_client.cpp`，添加eXosip日志级别：

```cpp
// 在initialize()函数中添加
eXosip_set_option(exosip_context, EXOSIP_OPT_SET_TLS_CERTIFICATES_INFO, nullptr);
eXosip_set_option(exosip_context, EXOSIP_OPT_UDP_KEEP_ALIVE, (void*)1);

// 启用调试日志
TRACE_INITIALIZE(6, nullptr);
```

### 使用网络抓包

```bash
# 抓取SIP信令
sudo tcpdump -i any -n port 5060 or port 5061 -w gb28181_sip.pcap

# 抓取RTP媒体流
sudo tcpdump -i any -n portrange 30000-30100 -w gb28181_rtp.pcap

# 使用wireshark分析
wireshark gb28181_sip.pcap
```

## Docker构建

### Dockerfile示例

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libopencv-dev \
    libexosip2-dev \
    libosip2-dev \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build . -j$(nproc)

EXPOSE 8080 5061/udp 30000-30100/udp

CMD ["./build/bin/detector_service"]
```

### 构建和运行

```bash
# 构建镜像
docker build -t detector_service:gb28181 .

# 运行容器
docker run -d \
  --name detector_service \
  -p 8080:8080 \
  -p 5061:5061/udp \
  -p 30000-30100:30000-30100/udp \
  detector_service:gb28181
```

## 性能测试

### 1. SIP注册压力测试

使用SIPp工具测试：
```bash
sipp -sn uac -s 34020000001320000001 192.168.1.100:5061
```

### 2. 并发推流测试

同时点播多个通道，监控系统资源：
```bash
# 监控CPU和内存
top -p $(pidof detector_service)

# 监控网络带宽
iftop -i eth0
```

### 3. 长时间稳定性测试

运行24小时以上，监控：
- 内存泄漏
- SIP注册状态
- 推流稳定性
- 日志错误

## 生产部署建议

### 1. 系统配置

```bash
# 增加UDP缓冲区大小
sudo sysctl -w net.core.rmem_max=26214400
sudo sysctl -w net.core.rmem_default=26214400
sudo sysctl -w net.core.wmem_max=26214400
sudo sysctl -w net.core.wmem_default=26214400

# 持久化配置
echo "net.core.rmem_max=26214400" | sudo tee -a /etc/sysctl.conf
echo "net.core.wmem_max=26214400" | sudo tee -a /etc/sysctl.conf
```

### 2. 防火墙配置

```bash
# 开放SIP端口
sudo ufw allow 5061/udp

# 开放RTP端口范围
sudo ufw allow 30000:30100/udp

# 重载防火墙
sudo ufw reload
```

### 3. 服务管理

创建systemd服务：
```ini
# /etc/systemd/system/detector_service.service
[Unit]
Description=Detector Service with GB28181
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/opt/detector_service
ExecStart=/opt/detector_service/bin/detector_service
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

启用服务：
```bash
sudo systemctl enable detector_service
sudo systemctl start detector_service
sudo systemctl status detector_service
```

## 故障排查清单

- [ ] eXosip2库已正确安装
- [ ] CMake配置成功找到eXosip2
- [ ] 程序编译链接成功
- [ ] 运行时没有库依赖错误
- [ ] GB28181配置正确
- [ ] SIP端口没有被占用
- [ ] 网络连接正常
- [ ] 防火墙规则正确
- [ ] SIP服务器可达
- [ ] 设备ID和密码正确
- [ ] 注册成功（查看日志）
- [ ] 心跳正常发送

## 下一步

构建成功后，参考以下文档继续：
- `GB28181_README.md` - 完整功能说明
- `GB28181_SIP_IMPLEMENTATION.md` - 实现细节
- 配置和使用GB28181服务

## 技术支持

如遇到问题：
1. 查看日志输出
2. 使用wireshark抓包分析
3. 检查系统资源
4. 参考故障排查清单
5. 查阅GB28181标准文档

