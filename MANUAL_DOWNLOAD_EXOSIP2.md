# 手动下载 eXosip2 和 osip2 说明

## 下载地址

### osip2
- **官方地址**: http://ftp.twaren.net/Unix/NonGNU/osip/
- **推荐版本**: libosip2-5.2.1.tar.gz
- **直接链接**: http://ftp.twaren.net/Unix/NonGNU/osip/libosip2-5.2.1.tar.gz

### eXosip2
- **官方地址**: http://download.savannah.gnu.org/releases/exosip/
- **推荐版本**: libeXosip2-5.3.0.tar.gz
- **直接链接**: http://download.savannah.gnu.org/releases/exosip/libeXosip2-5.3.0.tar.gz

## 放置位置

### 方法1: 放在构建目录（推荐）

将下载的 tar.gz 文件放在构建目录的 `third_party` 文件夹：

```bash
# 创建目录
mkdir -p build/linux-x64-Release/third_party

# 将下载的文件放在这里
# build/linux-x64-Release/third_party/libosip2-5.2.1.tar.gz
# build/linux-x64-Release/third_party/libeXosip2-5.3.0.tar.gz
```

然后修改 `cmake/FindExosip2.cmake`，使用本地文件：

```cmake
# 使用本地文件
set(OSIP2_URL "${CMAKE_BINARY_DIR}/third_party/libosip2-5.2.1.tar.gz")
set(EXOSIP2_URL "${CMAKE_BINARY_DIR}/third_party/libeXosip2-5.3.0.tar.gz")
```

### 方法2: 解压到源码目录

如果已经下载并解压了源码，可以直接放在源码目录：

```bash
# 创建目录
mkdir -p build/linux-x64-Release/third_party

# 解压 osip2
cd build/linux-x64-Release/third_party
tar -xzf /path/to/libosip2-5.2.1.tar.gz
mv libosip2-5.2.1 osip2

# 解压 eXosip2
tar -xzf /path/to/libeXosip2-5.3.0.tar.gz
mv libeXosip2-5.3.0 exosip2
```

然后修改 `cmake/FindExosip2.cmake`，跳过下载步骤：

```cmake
ExternalProject_Add(
    osip2
    SOURCE_DIR ${OSIP2_SOURCE_DIR}  # 使用已存在的源码
    DOWNLOAD_COMMAND ""  # 跳过下载
    UPDATE_COMMAND ""    # 跳过更新
    ...
)
```

## 快速操作步骤

### 步骤1: 下载文件

```bash
cd /mnt/d/Project/detector_service

# 创建目录
mkdir -p build/linux-x64-Release/third_party

# 下载 osip2
cd build/linux-x64-Release/third_party
wget http://ftp.twaren.net/Unix/NonGNU/osip/libosip2-5.2.1.tar.gz

# 下载 eXosip2
wget http://download.savannah.gnu.org/releases/exosip/libeXosip2-5.3.0.tar.gz
```

### 步骤2: 修改 CMake 配置

修改 `cmake/FindExosip2.cmake`，将 URL 改为本地文件路径：

```cmake
# 使用本地文件（如果已下载）
if(EXISTS "${CMAKE_BINARY_DIR}/third_party/libosip2-${OSIP2_VERSION}.tar.gz")
    set(OSIP2_URL "${CMAKE_BINARY_DIR}/third_party/libosip2-${OSIP2_VERSION}.tar.gz")
else()
    set(OSIP2_URL "http://ftp.twaren.net/Unix/NonGNU/osip/libosip2-${OSIP2_VERSION}.tar.gz")
endif()

if(EXISTS "${CMAKE_BINARY_DIR}/third_party/libeXosip2-${EXOSIP2_VERSION}.tar.gz")
    set(EXOSIP2_URL "${CMAKE_BINARY_DIR}/third_party/libeXosip2-${EXOSIP2_VERSION}.tar.gz")
else()
    set(EXOSIP2_URL "http://download.savannah.gnu.org/releases/exosip/libeXosip2-${EXOSIP2_VERSION}.tar.gz")
endif()
```

### 步骤3: 重新构建

```bash
./build.sh
```

## 目录结构

手动下载后的目录结构应该是：

```
build/linux-x64-Release/
└── third_party/
    ├── libosip2-5.2.1.tar.gz      # osip2 源码包
    ├── libeXosip2-5.3.0.tar.gz   # eXosip2 源码包
    ├── osip2/                     # 解压后的 osip2 源码（自动创建）
    ├── osip2-build/               # osip2 编译目录（自动创建）
    ├── exosip2/                   # 解压后的 eXosip2 源码（自动创建）
    ├── exosip2-build/             # eXosip2 编译目录（自动创建）
    └── exosip2-install/           # 安装目录（自动创建）
        ├── include/
        │   ├── osip2/
        │   └── eXosip2/
        └── lib/
            ├── libosip2.a
            ├── libosipparser2.a
            └── libeXosip2.a
```

## 验证下载

检查文件是否存在：

```bash
ls -lh build/linux-x64-Release/third_party/*.tar.gz
```

应该看到：
- `libosip2-5.2.1.tar.gz` (约 700KB)
- `libeXosip2-5.3.0.tar.gz` (约 200KB)

## 注意事项

1. **版本匹配**: 确保下载的版本与 `FindExosip2.cmake` 中设置的版本一致
2. **文件完整性**: 下载后可以验证文件大小，确保下载完整
3. **权限**: 确保文件有读取权限
4. **路径**: 使用绝对路径或相对于构建目录的路径

## 使用 curl 下载（如果 wget 不可用）

```bash
cd build/linux-x64-Release/third_party

# 下载 osip2
curl -L -o libosip2-5.2.1.tar.gz http://ftp.twaren.net/Unix/NonGNU/osip/libosip2-5.2.1.tar.gz

# 下载 eXosip2
curl -L -o libeXosip2-5.3.0.tar.gz http://download.savannah.gnu.org/releases/exosip/libeXosip2-5.3.0.tar.gz
```

