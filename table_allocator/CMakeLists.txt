cmake_minimum_required(VERSION 2.6)
project(table_allocator)

set (PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/shared/include)

include_directories("${PROJECT_INCLUDE_DIR}")

add_subdirectory(shared)
add_subdirectory(server)
