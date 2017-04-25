#ifndef TABLE_ALLOCATOR_CLIENT_H
#define TABLE_ALLOCATOR_CLIENT_H

#include <uv.h>

struct tac_ctx {
    uv_loop_t event_loop; 
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
    FILE *logfile;
    uint8_t use_syslog;
};

#endif
