# 配置库 (config) - 配置管理（仅头文件库）
add_library(config INTERFACE)

target_include_directories(config INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # 允许直接使用 #include "config.h"
)

target_compile_options(config INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

