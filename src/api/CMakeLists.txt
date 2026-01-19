# API 库 (libapi) - RESTful API 路由和 WebSocket 处理
add_library(api STATIC
    channel_api.cpp
    alert_api.cpp
    gb28181_config_api.cpp
    algorithm_config_api.cpp
    model_api.cpp
    ws_api_ix.cpp
    ws_handler.cpp
    report_config_api.cpp
)

target_include_directories(api PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(api PUBLIC
    config
    models
    database
    detector
    stream
    utils
    ${OpenCV_LIBS}
    Threads::Threads
)

# 链接 IXWebSocket（如果找到）
if(ixwebsocket_FOUND)
    target_link_libraries(api PUBLIC ixwebsocket::ixwebsocket)
elseif(IXWEBSOCKET_LIB)
    target_link_libraries(api PUBLIC ${IXWEBSOCKET_LIB})
elseif(IXWEBSOCKET_INCLUDE_DIR)
    # 如果只有头文件，尝试查找库
    find_library(IXWEBSOCKET_LIB ixwebsocket)
    if(IXWEBSOCKET_LIB)
        target_link_libraries(api PUBLIC ${IXWEBSOCKET_LIB})
    else()
        # IXWebSocket 可能是 header-only，只需要包含路径
        message(STATUS "IXWebSocket 作为 header-only 库使用")
    endif()
endif()

# 链接 OpenSSL（用于 WebSocket SHA-1 和 Base64）
if(OPENSSL_FOUND)
    target_link_libraries(api PUBLIC OpenSSL::SSL OpenSSL::Crypto)
else()
    # 尝试查找 OpenSSL
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(api PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    else()
        # 使用 vcpkg 的 OpenSSL
        target_link_libraries(api PUBLIC ssl crypto)
    endif()
endif()

target_compile_options(api PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# 设置位置无关代码，以便静态库可以链接到共享库
set_target_properties(api PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

