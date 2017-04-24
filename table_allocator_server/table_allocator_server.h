#ifndef TABLE_ALLOCATOR_SERVER_H
#define TABLE_ALLOCATOR_SERVER_H

#include <uv.h>

//move to shared
#define DOMAIN_SOCKET_TIMEOUT_MS 200

struct tas_ctx {
    uv_loop_t event_loop;
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
    FILE *logfile;
    uint8_t use_syslog;
};

#endif
