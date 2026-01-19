# 检测器库 (libdetector) - YOLOv11 检测器
# 如果启用完全静态链接，使用静态库；否则使用共享库

# BM1684平台检测器支持
if(ENABLE_BM1684)
    # BM1684 模式：只编译 BM1684 检测器
    if(STATIC_LINK_ALL)
        add_library(detector STATIC
            yolov11_detector_bm1684.cpp
        )
    else()
        add_library(detector SHARED
            yolov11_detector_bm1684.cpp
        )
    endif()
    
    target_include_directories(detector PUBLIC
        ${BM1684_INCLUDE_DIRS}
    )
    target_link_directories(detector PUBLIC
        ${BM1684_LIB_DIRS}
    )
    target_link_libraries(detector PUBLIC
        ${BM1684_LIBS}
        pthread
    )
else()
    # 非 BM1684 模式：只编译标准检测器（使用 onnxruntime）
    if(STATIC_LINK_ALL)
        add_library(detector STATIC
            yolov11_detector.cpp
        )
    else()
        add_library(detector SHARED
            yolov11_detector.cpp
        )
    endif()
endif()

target_include_directories(detector PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# 在 BM1684 模式下，使用 SDK 提供的 OpenCV
if(ENABLE_BM1684)
    target_include_directories(detector PUBLIC
        ${BM1684_INCLUDE_DIRS}  # 包含 bmcv_api.h 等 BM1684 SDK 头文件
        ${BM1684_OPENCV_INCLUDE_DIRS}
        ${BM1684_FFMPEG_INCLUDE_DIRS}  # OpenCV 头文件依赖 FFmpeg 头文件
    )
    target_link_directories(detector PUBLIC
        ${BM1684_OPENCV_LIB_DIRS}
    )
    target_link_libraries(detector PUBLIC
        ${BM1684_OPENCV_LIBS}
        config
        utils
        models
    )
else()
    target_link_libraries(detector PUBLIC
        ${OpenCV_LIBS}
        config
        utils
        models
    )
endif()

# onnxruntime 仅在非 BM1684 模式下链接
if(NOT ENABLE_BM1684)
    target_link_libraries(detector PUBLIC
        onnxruntime::onnxruntime
    )
endif()

# 显式设置 C++ 标准（使用全局设置的值，确保兼容性）
set_target_properties(detector PROPERTIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_options(detector PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

