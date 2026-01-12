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

# 查找FFmpeg库 - 优先使用vcpkg的FFmpeg包
find_package(unofficial-ffmpeg QUIET)
if(unofficial-ffmpeg_FOUND)
    message(STATUS "Found FFmpeg via vcpkg")
    target_link_libraries(stream PUBLIC
        unofficial::ffmpeg::avcodec
        unofficial::ffmpeg::avformat
        unofficial::ffmpeg::avutil
        unofficial::ffmpeg::swscale
    )
else()
    # 回退到手动查找FFmpeg库
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
    find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
    find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
    find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)

    find_library(AVCODEC_LIBRARY avcodec)
    find_library(AVFORMAT_LIBRARY avformat)
    find_library(AVUTIL_LIBRARY avutil)
    find_library(SWSCALE_LIBRARY swscale)

    if(AVCODEC_INCLUDE_DIR AND AVFORMAT_INCLUDE_DIR AND AVUTIL_INCLUDE_DIR AND SWSCALE_INCLUDE_DIR
       AND AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY AND SWSCALE_LIBRARY)
        message(STATUS "Found FFmpeg libraries manually")
        target_include_directories(stream PUBLIC
            ${AVCODEC_INCLUDE_DIR}
            ${AVFORMAT_INCLUDE_DIR}
            ${AVUTIL_INCLUDE_DIR}
            ${SWSCALE_INCLUDE_DIR}
        )
        # FFmpeg库的链接顺序很重要：avformat -> avcodec -> avutil -> swscale
        # 但实际链接时，依赖关系是反向的，所以应该先链接被依赖的库
        target_link_libraries(stream PUBLIC
            ${SWSCALE_LIBRARY}
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY}
        )
    else()
        message(WARNING "FFmpeg libraries not found, RTMP push stream may not work")
    endif()
endif()

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

