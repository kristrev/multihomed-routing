#Standard stuff
cmake_minimum_required(VERSION 2.6)
project(table_allocator_shared)

set(CMAKE_C_FLAGS "-O1 -Wall -std=gnu99 -g")

set (PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set (PROJECT_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

set(SOURCE
	${PROJECT_SRC_DIR}/table_allocator_socket_helpers.c
	${PROJECT_SRC_DIR}/table_allocator_libuv_helpers.c
	${PROJECT_SRC_DIR}/table_allocator_shared_json.c
) 

include_directories("${PROJECT_INCLUDE_DIR}")
add_library(${PROJECT_NAME} STATIC ${SOURCE})
