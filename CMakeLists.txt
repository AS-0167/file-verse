cmake_minimum_required(VERSION 3.15)
project(file_verse_ofs CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ofs_core
    source/core/config.cpp
    source/core/file_io.cpp
    source/core/fs_format.cpp
    source/core/fs_init.cpp
    source/data_structures/fifo_queue.h
    source/data_structures/dynamic_array.hpp
    source/data_structures/linked_list.hpp
    source/data_structures/bitmap.hpp
    source/data_structures/hashmap.hpp
)

target_include_directories(ofs_core PUBLIC ${CMAKE_SOURCE_DIR}/source/include)

add_executable(ofs_server source/server/main.cpp)
target_link_libraries(ofs_server PRIVATE ofs_core)

# Test data structures (optional)
add_executable(test_ds source/server/test_ds.cpp)
target_link_libraries(test_ds PRIVATE ofs_core)

