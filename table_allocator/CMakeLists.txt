cmake_minimum_required(VERSION 2.6)
project(table_allocator)

set (PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/shared/include)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories("${PROJECT_INCLUDE_DIR}")

add_subdirectory(server)
