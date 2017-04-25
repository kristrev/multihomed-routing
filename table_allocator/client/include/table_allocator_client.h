#ifndef TABLE_ALLOCATOR_CLIENT_H
#define TABLE_ALLOCATOR_CLIENT_H

#include <uv.h>

#define REQUEST_RETRANSMISSION_MS 2000

struct tac_ctx {
    uv_loop_t event_loop; 
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
	uv_timer_t request_timeout_handle;
    FILE *logfile;
    uint8_t use_syslog;
};

#endif
