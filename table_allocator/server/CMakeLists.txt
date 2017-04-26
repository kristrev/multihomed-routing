#Standard stuff
cmake_minimum_required(VERSION 2.6)
project(table_allocator_server)

set(CMAKE_C_FLAGS "-O1 -Wall -std=gnu99 -g")

set (PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set (PROJECT_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

find_library(LIBUV_LIBRARY uv)
find_library(LIBSQLITE3_LIBRARY sqlite3)
find_library(LIBJSON_LIBRARY json-c)

set(SOURCE
    ${PROJECT_SRC_DIR}/table_allocator_server.c
    ${PROJECT_SRC_DIR}/table_allocator_server_sockets.c
) 

include_directories("${PROJECT_INCLUDE_DIR}")
add_executable(${PROJECT_NAME} ${SOURCE})
target_link_libraries(${PROJECT_NAME} table_allocator_shared)
target_link_libraries(${PROJECT_NAME} ${LIBUV_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${LIBSQLITE3_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${LIBJSON_LIBRARY})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION usr/bin)
