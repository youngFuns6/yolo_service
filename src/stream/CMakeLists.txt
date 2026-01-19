# 推流库 (libstream) - 视频推流管理和帧回调处理
add_library(stream STATIC
    stream_manager.cpp
    frame_callback.cpp
    gb28181_streamer.cpp
    gb28181_sip_client.cpp
)

# BM1684平台视频编解码器支持
if(ENABLE_BM1684)
    target_sources(stream PRIVATE
        bm1684_video_decoder.cpp
        bm1684_video_encoder.cpp
    )
    target_include_directories(stream PUBLIC
        ${BM1684_INCLUDE_DIRS}
    )
    target_link_directories(stream PUBLIC
        ${BM1684_FFMPEG_LIB_DIRS}
    )
endif()

target_include_directories(stream PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/utils/include
    ${OpenCV_INCLUDE_DIRS}
)

# 在 BM1684 模式下，使用 SDK 提供的 OpenCV 和 FFmpeg
if(ENABLE_BM1684)
    target_include_directories(stream PUBLIC
        ${BM1684_INCLUDE_DIRS}  # 包含 bmcv_api.h 等 BM1684 SDK 头文件
        ${BM1684_OPENCV_INCLUDE_DIRS}
        ${BM1684_FFMPEG_INCLUDE_DIRS}
    )
    target_link_directories(stream PUBLIC
        ${BM1684_OPENCV_LIB_DIRS}
        ${BM1684_FFMPEG_LIB_DIRS}
    )
    target_link_libraries(stream PUBLIC
        ${BM1684_OPENCV_LIBS}
        ${BM1684_FFMPEG_LIBS}
    )
    message(STATUS "使用 BM1684 SDK 提供的 OpenCV 和 FFmpeg")
else()
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
    endif()  # 结束 unofficial-ffmpeg_FOUND 的 else 分支
    # 在非 BM1684 模式下，链接 OpenCV
    target_link_libraries(stream PUBLIC
        ${OpenCV_LIBS}
    )
endif()  # 结束 ENABLE_BM1684 的 else 分支

# 链接eXosip2库（由主CMakeLists.txt的FindExosip2模块处理）
if(EXOSIP2_FOUND)
    # 设置包含目录
    if(EXOSIP2_INCLUDE_DIRS)
        target_include_directories(stream PUBLIC ${EXOSIP2_INCLUDE_DIRS})
    endif()
    
    if(TARGET exosip2::exosip2)
        # 使用导入目标（从源码编译）
        message(STATUS "Linking eXosip2 from source build")
        
        # 确保 ExternalProject 先构建
        if(TARGET exosip2_libs_ready)
            add_dependencies(stream exosip2_libs_ready)
        elseif(TARGET exosip2 AND TARGET osip2)
            add_dependencies(stream exosip2 osip2)
        endif()
        
        # 使用导入目标（会自动处理依赖关系）
        target_link_libraries(stream PUBLIC
            exosip2::exosip2
            osip2::osip2
            osipparser2::osipparser2
        )
    else()
        # 使用系统安装的库
        message(STATUS "Linking eXosip2 from system")
        target_link_libraries(stream PUBLIC ${EXOSIP2_LIBRARIES})
    endif()
    # macOS 需要链接 libresolv 用于 DNS 解析（eXosip2 需要）
    if(APPLE AND EXOSIP2_FOUND)
        target_link_libraries(stream PUBLIC resolv)
    endif()
else()
    message(WARNING "eXosip2 not found, GB28181 SIP signaling will not work")
    # 添加编译定义以禁用GB28181 SIP功能
    target_compile_definitions(stream PRIVATE DISABLE_GB28181_SIP)
endif()

target_link_libraries(stream PUBLIC
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

