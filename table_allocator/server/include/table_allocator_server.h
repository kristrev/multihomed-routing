/*
 * Copyright 2017 Kristian Evensen <kristian.evensen@gmail.com>
 *
 * This file is part of Usb Monitor. Usb Monitor is free software: you can
 * redistribute it and/or modify it under the terms of the Lesser GNU General
 * Public License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * Usb Montior is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Usb Monitor. If not, see http://www.gnu.org/licenses/.
 */

#ifndef TABLE_ALLOCATOR_SERVER_H
#define TABLE_ALLOCATOR_SERVER_H

#include <uv.h>
#include <linux/if.h>
#include <sqlite3.h>

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

//check for dead leases every fifth minute
#define DEAD_LEASE_TIMEOUT  300000

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
    uv_timer_t dead_leases_timeout_handle;
    FILE *logfile;

    //different database pointers/handlers
    sqlite3 *db_handle;
    sqlite3_stmt *insert_rt_table;
    sqlite3_stmt *select_rt_table;
    sqlite3_stmt *delete_rt_table;
    sqlite3_stmt *update_rt_table;
    sqlite3_stmt *select_dead_leases;
    sqlite3_stmt *delete_dead_leases;

    //current we only support one request
    struct tas_client_req *req;

    //in memory table map
    uint32_t *tables_inet;
    uint32_t *tables_inet6;
    uint32_t *tables_unspec;
    uint32_t num_table_elements;
    uint32_t num_tables;
    uint32_t table_offset;

    //paths (we don't carry the logfile around)
    uint8_t socket_path[TA_SHARED_MAX_ADDR_SIZE];
    uint8_t db_path[MAX_DB_PATH_LEN];

    //todo: allocate this separatly?
    uint8_t client_req_buffer[CLIENT_REQ_BUFFER_SIZE];
    uint16_t table_timeout;
    uint8_t use_syslog;
};

#endif
