# eXosip2 自动编译说明

## 🎯 快速开始

项目已配置为**自动从源码编译 eXosip2 和 osip2**，无需手动安装！

### 1. 安装构建工具（仅首次需要）

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install -y build-essential autoconf automake libtool git pkg-config
```

#### Windows (MSYS2)
```bash
pacman -S base-devel autoconf automake libtool git pkg-config
```

#### macOS
```bash
brew install autoconf automake libtool pkg-config
```

### 2. 正常构建项目

```bash
./build.sh
```

**就这么简单！** CMake 会自动：
- ✅ 检测系统是否已安装 eXosip2
- ✅ 如果未安装，自动从 GitLab 下载源码
- ✅ 自动编译 osip2 和 eXosip2
- ✅ 自动链接到项目

## 📋 工作原理

1. **首次构建**：CMake 会下载并编译 eXosip2（约 5-10 分钟）
2. **后续构建**：使用缓存的编译产物（快速）
3. **跨平台**：支持 Linux、Windows (MinGW)、macOS

## 🔍 编译位置

编译后的库位于：
```
build/linux-x64-Release/third_party/exosip2-install/
├── include/
│   ├── eXosip2/
│   └── osip2/
└── lib/
    ├── libeXosip2.a
    ├── libosip2.a
    └── libosipparser2.a
```

## ⚙️ 高级选项

### 强制从源码编译
```bash
cmake .. -DEXOSIP2_BUILD_FROM_SOURCE=ON
```

### 使用系统已安装的库
```bash
cmake .. -DEXOSIP2_BUILD_FROM_SOURCE=OFF
```

## 🐛 常见问题

### Q: Git 下载失败？
**A**: 检查网络连接，或使用代理：
```bash
git config --global http.proxy http://proxy:port
```

### Q: configure 失败？
**A**: 确保安装了 autotools：
```bash
sudo apt-get install autoconf automake libtool
```

### Q: 编译很慢？
**A**: 首次编译需要下载和编译两个库，后续会使用缓存。

## 📚 更多信息

- [BUILD_EXOSIP2.md](BUILD_EXOSIP2.md) - 详细构建说明
- [GB28181_README.md](GB28181_README.md) - GB28181功能文档

