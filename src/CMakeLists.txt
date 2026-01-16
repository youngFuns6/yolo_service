# 主程序 - 只包含 main.cpp
add_executable(${PROJECT_NAME}
    main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
    unofficial::sqlite3::sqlite3
    Crow::Crow
    utils
    models
    database
    detector
    stream
    api
    service
)

# macOS 上需要链接 Security framework（用于 eXosip2）
if(APPLE)
    find_library(SECURITY_FRAMEWORK Security)
    if(SECURITY_FRAMEWORK)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_FRAMEWORK})
    endif()
endif()

# 确保 eXosip2 ExternalProject 先构建（如果从源码编译）
if(TARGET exosip2_libs_ready)
    add_dependencies(${PROJECT_NAME} exosip2_libs_ready)
elseif(TARGET exosip2 AND TARGET osip2)
    add_dependencies(${PROJECT_NAME} exosip2 osip2)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Linux 静态链接配置 - 解决 GLIBC 版本不匹配问题
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 静态链接 libgcc 和 libstdc++，避免 GLIBC 版本问题
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
    )
    
    # 如果启用了完全静态链接选项，添加 -static
    if(STATIC_LINK_ALL)
        message(STATUS "为 ${PROJECT_NAME} 启用完全静态链接")
        target_link_options(${PROJECT_NAME} PRIVATE -static)
    endif()
endif()

