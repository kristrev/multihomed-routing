#ifndef TABLE_ALLOCATOR_SERVER_H
#define TABLE_ALLOCATOR_SERVER_H

#include <uv.h>
#include <linux/if.h>

#include "table_allocator_shared_json.h"

//Maximum number of tables we can allocated
//todo: make configurable?
#define MAX_NUM_TABLES    4096

//comput the number of entries we need in the table, done by diving the maximum
//number of elements on 32
#if (MAX_NUM_TABLES < 32)
    #define NUM_TABLE_ELEMENTS 1
//If number of values is not divisible by 32, I need an additional element to
//store the remainders.
#elif (MAX_NUM_TABLES & 0x1F)
    #define NUM_TABLE_ELEMENTS ((MAX_NUM_TABLES >> 5) + 1)
#else
    #define NUM_TABLE_ELEMENTS (MAX_NUM_TABLES >> 5)
#endif

#define CLIENT_REQ_BUFFER_SIZE 512

struct tas_client_req {
    char ifname[IFNAMSIZ];
    char tag[TA_SHARED_MAX_TAG_SIZE];
    char address[INET6_ADDRSTRLEN];
    uint8_t ver;
    uint8_t cmd;
    uint8_t addr_family;
};

struct tas_ctx {
    uv_loop_t event_loop;
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
    FILE *logfile;
    struct tas_client_req *req;
    //todo: allocate these separatly?
    uint32_t tables_inet[NUM_TABLE_ELEMENTS];
    uint32_t tables_inet6[NUM_TABLE_ELEMENTS];
    uint32_t tables_unspec[NUM_TABLE_ELEMENTS];
    //todo: allocate this separatly?
    uint8_t client_req_buffer[CLIENT_REQ_BUFFER_SIZE];
    uint8_t use_syslog;
};

#endif
