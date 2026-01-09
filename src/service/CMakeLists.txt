# 服务库 (libservice) - 服务初始化和启动逻辑
# 使用静态库以避免 FFmpeg 静态库链接到共享库时的 -fPIC 问题
add_library(service STATIC
    service.cpp
)

target_include_directories(service PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(service PUBLIC
    config
    database
    detector
    stream
    api
)

target_compile_options(service PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

