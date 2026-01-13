# 模型库 (libmodels) - 通道管理和推流配置管理
add_library(models SHARED
    channel.cpp
    gb28181_config.cpp
    alert.cpp
    algorithm_config.cpp
    report_config.cpp
)

target_include_directories(models PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/utils/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(models PUBLIC
    Threads::Threads
    database
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
)

target_compile_options(models PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

