# 工具库 (libutils) - 基础工具函数
# 如果启用完全静态链接，使用静态库；否则使用共享库
if(STATIC_LINK_ALL)
    add_library(utils STATIC
        image_utils.cpp
        report_service.cpp
    )
else()
    add_library(utils SHARED
        image_utils.cpp
        report_service.cpp
    )
endif()

target_include_directories(utils PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/models/include
    ${OpenCV_INCLUDE_DIRS}
)

# 如果 FFmpeg 可用，添加其包含路径（用于 ffmpeg_utils.h）
find_package(unofficial-ffmpeg QUIET)
if(unofficial-ffmpeg_FOUND)
    # vcpkg 的 FFmpeg 包会自动设置包含路径
    target_link_libraries(utils PUBLIC unofficial::ffmpeg::avutil)
else()
    # 尝试手动查找 FFmpeg 头文件
    find_path(AVUTIL_INCLUDE_DIR libavutil/error.h)
    if(AVUTIL_INCLUDE_DIR)
        target_include_directories(utils PUBLIC ${AVUTIL_INCLUDE_DIR})
    endif()
endif()

target_link_libraries(utils PUBLIC
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    unofficial::mosquitto::mosquitto
    models
)

target_compile_options(utils PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

