#ifndef TABLE_ALLOCATOR_CLIENT_H
#define TABLE_ALLOCATOR_CLIENT_H

#include <uv.h>
#include <linux/if.h>

#define REQUEST_RETRANSMISSION_MS   2000

#define MAX_TAG_SIZE                128
#define MAX_ADDR_SIZE               64

struct tac_ctx {
    uv_loop_t event_loop; 
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
	uv_timer_t request_timeout_handle;
    FILE *logfile;
    char ifname[IFNAMSIZ];
    char tag[MAX_TAG_SIZE];
    char address[MAX_ADDR_SIZE];
    char destination[MAX_ADDR_SIZE];
    uint8_t use_syslog;
    uint8_t addr_family;
};

#endif
