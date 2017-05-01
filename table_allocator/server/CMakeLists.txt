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
    ${PROJECT_SRC_DIR}/table_allocator_server_clients.c
    ${PROJECT_SRC_DIR}/table_allocator_server_sqlite.c
) 

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kristian Evensen <kristian.evensen@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_NAME "table_allocator_server")
set(CPACK_DEBIAN_PACKAGE_VERSION "0.1-1")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libuv libjson-c3 libsqlite3-0")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "This package contains the Table Allocator
Server, for use together with the table allocator client. The purpose of this
tool is to distribute and keep track of routing tables on multihomed hosts.
Clients will request tables using a basic, well-defined protocols and are then
required to refresh them at some configurable interval. The server will free any
dead table leases.")
INCLUDE(CPack)

#required for individual builds
if(NOT TARGET table_allocator_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared
        ${CMAKE_CURRENT_BINARY_DIR}/shared)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../shared/include")
endif()

include_directories("${PROJECT_INCLUDE_DIR}")
add_executable(${PROJECT_NAME} ${SOURCE})
target_link_libraries(${PROJECT_NAME} table_allocator_shared)
target_link_libraries(${PROJECT_NAME} ${LIBUV_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${LIBSQLITE3_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${LIBJSON_LIBRARY})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION usr/sbin)
