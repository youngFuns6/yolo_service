# 数据库库 (libdatabase) - SQLite 数据库操作
add_library(database SHARED
    database.cpp
)

target_include_directories(database PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/models/include
)

target_link_libraries(database PUBLIC
    unofficial::sqlite3::sqlite3
)

target_compile_options(database PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

