# 使用 IXWebSocket 库实现 WebSocket

## 概述

由于自定义 WebSocket 实现连接失败，已改用成熟的第三方库 **IXWebSocket**。

## 已完成的更改

1. **添加依赖**
   - 在 `vcpkg.json` 中添加了 `ixwebsocket`

2. **创建包装类**
   - `src/api/include/websocket_wrapper_ix.h`: IXWebSocket 的包装类，保持与现有 `ws_handler` 的接口兼容

3. **重写 WebSocket API**
   - `src/api/ws_api_ix.cpp`: 使用 IXWebSocket 实现 WebSocket 服务器

4. **更新构建配置**
   - `src/api/CMakeLists.txt`: 使用 `ws_api_ix.cpp` 替代 `ws_api.cpp`
   - `CMakeLists.txt`: 添加 IXWebSocket 的查找和链接逻辑

## 编译步骤

1. **配置 CMake**（会自动安装 ixwebsocket）
   ```bash
   cmake -B build/macos-arm64-Release -S . \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ```

2. **编译项目**
   ```bash
   cmake --build build/macos-arm64-Release --target detector_service
   ```

3. **如果编译失败**，可能需要手动安装 ixwebsocket：
   ```bash
   # 如果 vcpkg 在 PATH 中
   vcpkg install ixwebsocket
   
   # 或者通过 CMake 配置时自动安装
   ```

## 测试

启动服务器后，应该看到：
```
WebSocket 服务器已启动在端口 9091 (使用 IXWebSocket)
```

然后运行测试：
```bash
./test_ws_quick.sh
# 或
python3 test_websocket.py
```

## 优势

- ✅ 使用成熟的第三方库，稳定可靠
- ✅ 自动处理 WebSocket 握手、帧解析等复杂逻辑
- ✅ 支持多平台（Linux、Windows、macOS）
- ✅ 保持与现有 `ws_handler` 接口的兼容性

## 注意事项

- IXWebSocket 的 API 可能与示例略有不同，如果编译出错，可能需要根据实际 API 调整代码
- 路径匹配：IXWebSocket 通过 URI 来区分不同的 WebSocket 端点
- 端口：WebSocket 服务器使用 HTTP 端口 + 1（例如 HTTP 9090，WebSocket 9091）
