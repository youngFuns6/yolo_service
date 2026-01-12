# 检测器库 (libdetector) - YOLOv11 检测器
add_library(detector SHARED
    yolov11_detector.cpp
)

target_include_directories(detector PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(detector PUBLIC
    ${OpenCV_LIBS}
    onnxruntime::onnxruntime
    utils
    models
)

target_compile_options(detector PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

