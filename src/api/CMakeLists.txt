# API 库 (libapi) - RESTful API 路由和 WebSocket 处理
add_library(api STATIC
    channel_api.cpp
    alert_api.cpp
    gb28181_config_api.cpp
    algorithm_config_api.cpp
    model_api.cpp
    ws_api.cpp
    ws_handler.cpp
    report_config_api.cpp
)

target_include_directories(api PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(api PUBLIC
    Crow::Crow
    config
    models
    database
    detector
    stream
    utils
    ${OpenCV_LIBS}
)

target_compile_options(api PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# 设置位置无关代码，以便静态库可以链接到共享库
set_target_properties(api PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

