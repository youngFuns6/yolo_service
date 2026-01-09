# 推流库 (libstream) - 视频推流管理和帧回调处理
add_library(stream STATIC
    stream_manager.cpp
    frame_callback.cpp
)

target_include_directories(stream PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(stream PUBLIC
    ${OpenCV_LIBS}
    Threads::Threads
    config
    models
    detector
    utils
    api
    database
)

target_compile_options(stream PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# 设置位置无关代码，以便静态库可以链接到共享库
set_target_properties(stream PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

