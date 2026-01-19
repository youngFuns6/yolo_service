# 数据库库 (libdatabase) - SQLite 数据库操作
# 如果启用完全静态链接，使用静态库；否则使用共享库
if(STATIC_LINK_ALL)
    add_library(database STATIC
        database.cpp
    )
else()
    add_library(database SHARED
        database.cpp
    )
endif()

target_include_directories(database PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/models/include
    ${CMAKE_SOURCE_DIR}/src/utils/include
)

target_link_libraries(database PUBLIC
    unofficial::sqlite3::sqlite3
)

# 显式设置 C++ 标准（使用全局设置的值，确保兼容性）
set_target_properties(database PROPERTIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_options(database PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

