#ifndef TABLE_ALLOCATOR_SERVER_H
#define TABLE_ALLOCATOR_SERVER_H

#include <uv.h>
#include <linux/if.h>

#include "table_allocator_shared_json.h"

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
    uint8_t client_req_buffer[CLIENT_REQ_BUFFER_SIZE];
    uint8_t use_syslog;
};

#endif
