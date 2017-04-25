#Standard stuff
cmake_minimum_required(VERSION 2.6)
project(table_allocator_server)

set(CMAKE_C_FLAGS "-O1 -Wall -std=gnu99 -g")

set (PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set (PROJECT_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

find_library(LIBMNL_LIBRARY mnl)
find_library(LIBUV_LIBRARY uv)
find_library(LIBSQLITE3_LIBRARY sqlite3)

set(SOURCE
    ${PROJECT_SRC_DIR}/table_allocator_server.c
    ${PROJECT_SRC_DIR}/table_allocator_socket_helpers.c
) 

include_directories("${PROJECT_INCLUDE_DIR}")
add_executable(table_allocator_server ${SOURCE})
target_link_libraries(table_allocator_server ${LIBMNL_LIBRARY})
target_link_libraries(table_allocator_server ${LIBUV_LIBRARY})
target_link_libraries(table_allocator_server ${LIBSQLITE3_LIBRARY})
install(TARGETS table_allocator_server RUNTIME DESTINATION sbin)
