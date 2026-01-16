# 检测器库 (libdetector) - YOLOv11 检测器
# 如果启用完全静态链接，使用静态库；否则使用共享库
if(STATIC_LINK_ALL)
    add_library(detector STATIC
        yolov11_detector.cpp
    )
else()
    add_library(detector SHARED
        yolov11_detector.cpp
    )
endif()

target_include_directories(detector PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(detector PUBLIC
    ${OpenCV_LIBS}
    onnxruntime::onnxruntime
    config
    utils
    models
)

target_compile_options(detector PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

