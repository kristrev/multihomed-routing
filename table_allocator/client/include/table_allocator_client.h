#ifndef TABLE_ALLOCATOR_CLIENT_H
#define TABLE_ALLOCATOR_CLIENT_H

#include <uv.h>
#include <linux/if.h>

#include <table_allocator_shared_json.h>

#define REQUEST_RETRANSMISSION_MS   2000

#define MAX_TAG_SIZE                128
//max. len. for unix domain sockets is 108, but string needs to be zero
//teminated and we loose one byte (in front) since we use abstract naming
#define MAX_ADDR_SIZE               106

struct tac_ctx {
    uv_loop_t event_loop; 
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
	uv_timer_t request_timeout_handle;
    FILE *logfile;
    char ifname[IFNAMSIZ];
    char tag[MAX_TAG_SIZE];
    char address[INET6_ADDRSTRLEN];
    char destination[MAX_ADDR_SIZE];
    uint8_t rcv_buf[TA_SHARE_MAX_JSON_LEN];
    uint8_t use_syslog;
    uint8_t addr_family;
};

#endif
