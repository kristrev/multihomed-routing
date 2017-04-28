#ifndef TABLE_ALLOCATOR_SERVER_H
#define TABLE_ALLOCATOR_SERVER_H

#include <uv.h>
#include <linux/if.h>

#include <table_allocator_shared_json.h>

#define TAS_SOCKET_PATH_KEY     "socket_path"
#define TAS_TABLE_OFFSET_KEY    "table_offset"
#define TAS_NUM_TABLES_KEY  "num_tables"
#define TAS_TABLE_TIMEOUT_KEY   "table_timeout"
#define TAS_DB_PATH_KEY         "db_path"
#define TAS_DO_SYSLOG_KEY       "do_syslog"
#define TAS_LOG_PATH_KEY        "log_path"
#define TAS_ADDR_FAMILIES_KEY   "addr_families"

#define TAS_ADDR_FAMILIES_INET_KEY      "inet"
#define TAS_ADDR_FAMILIES_INET6_KEY     "inet6"
#define TAS_ADDR_FAMILIES_UNSPEC_KEY    "unspec"

#define CLIENT_REQ_BUFFER_SIZE  512
#define MAX_DB_PATH_LEN         256

#define ADDR_FAMILY_INET    0x1
#define ADDR_FAMILY_INET6   0x2
#define ADDR_FAMILY_UNSPEC  0x4

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
    uint32_t *tables_inet;
    uint32_t *tables_inet6;
    uint32_t *tables_unspec;
    uint32_t num_table_elements;
    uint32_t num_tables;
    uint32_t table_offset;
    uint8_t socket_path[TA_SHARED_MAX_ADDR_SIZE];
    //todo: consider if I should bother carrying this around
    uint8_t db_path[MAX_DB_PATH_LEN];
    //todo: allocate this separatly?
    uint8_t client_req_buffer[CLIENT_REQ_BUFFER_SIZE];
    uint16_t table_timeout;
    uint8_t use_syslog;
};

#endif
