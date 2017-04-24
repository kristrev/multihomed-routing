#Standard stuff
cmake_minimum_required(VERSION 2.6)
project(table_allocator_server)

set(CMAKE_C_FLAGS "-O1 -Wall -std=gnu99 -g")

set(LIBS mnl json-c uv sqlite3)
set(SOURCE
    table_allocator_server.c
    table_allocator_socket_helpers.c
) 

add_executable(table_allocator_server ${SOURCE})
target_link_libraries(table_allocator_server ${LIBS})
install(TARGETS table_allocator_server RUNTIME DESTINATION sbin)
