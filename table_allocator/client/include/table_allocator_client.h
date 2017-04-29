#ifndef TABLE_ALLOCATOR_CLIENT_H
#define TABLE_ALLOCATOR_CLIENT_H

#include <uv.h>
#include <linux/if.h>

#include <table_allocator_shared_json.h>

#define REQUEST_RETRANSMISSION_MS   2000

struct tac_ctx {
    uv_loop_t event_loop; 
    uv_udp_t unix_socket_handle;
	uv_timer_t unix_socket_timeout_handle;
	uv_timer_t request_timeout_handle;
    FILE *logfile;
    char ifname[IFNAMSIZ];
    char tag[TA_SHARED_MAX_TAG_SIZE];
    char address[INET6_ADDRSTRLEN];
    char destination[TA_SHARED_MAX_ADDR_SIZE];
    uint8_t rcv_buf[TA_SHARED_MAX_JSON_LEN];
    uint8_t use_syslog;
    uint8_t addr_family;
    //request or release
    uint8_t cmd;
};

#endif
