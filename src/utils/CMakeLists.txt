# 工具库 (libutils) - 基础工具函数
add_library(utils SHARED
    image_utils.cpp
)

target_include_directories(utils PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(utils PUBLIC
    ${OpenCV_LIBS}
)

target_compile_options(utils PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

