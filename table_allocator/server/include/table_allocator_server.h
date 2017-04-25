#ifndef TABLE_ALLOCATOR_SERVER_H
#define TABLE_ALLOCATOR_SERVER_H

#include <uv.h>

#define CLIENT_REQ_BUFFER_SIZE 512

struct tas_ctx {
    uv_loop_t event_loop;
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
    //todo: will be moved to client struct?
    uint8_t client_req_buffer[CLIENT_REQ_BUFFER_SIZE];
    FILE *logfile;
    uint8_t use_syslog;
};

#endif
