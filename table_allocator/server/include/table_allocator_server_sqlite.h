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

#ifndef TABLE_ALLOCATOR_SERVER_SQLITE_H
#define TABLE_ALLOCATOR_SERVER_SQLITE_H

#include <sqlite3.h>
#include <stdint.h>
#include <time.h>

#define CREATE_SQL          "CREATE TABLE IF NOT EXISTS RtTables(" \
                            "RtTable INTEGER NOT NULL," \
                            "AddrFamily INTEGER NOT NULL," \
                            "Ifname TEXT NOT NULL," \
                            "Addr TEXT NOT NULL," \
                            "LeaseExpires INTEGER NOT NULL,"\
                            "Tag TEXT," \
                            "PRIMARY KEY(RtTable,AddrFamily)"\
                            "ON CONFLICT REPLACE)"

#define SELECT_RT_TABLE     "SELECT "\
                                "RtTable "\
                            "FROM "\
                                "RtTables "\
                            "WHERE "\
                                "AddrFamily=? AND Ifname=? AND Addr=?"

#define INSERT_RT_TABLE     "INSERT INTO "\
                                "RtTables (RtTable,AddrFamily,Ifname,Addr,"\
                                "LeaseExpires, Tag) "\
                            "VALUES "\
                                "(?,?,?,?,?,?)"

#define DELETE_RT_TABLE     "DELETE FROM "\
                                "RtTables "\
                            "WHERE "\
                                "AddrFamily=? AND Ifname=? AND Addr=?"

#define UPDATE_RT_TABLE     "UPDATE "\
                                "RtTables "\
                            "SET "\
                                "LeaseExpires=? "\
                            "WHERE "\
                                "AddrFamily=? AND RtTable=?"

#define SELECT_DEAD_LEASES  "SELECT "\
                                "RtTable,AddrFamily "\
                            "FROM "\
                                "RtTables "\
                            "WHERE "\
                                "LeaseExpires <= ?"

#define DELETE_DEAD_LEASES  "DELETE FROM "\
                                "RtTables "\
                            "WHERE "\
                                "LeaseExpires <= ?"

//used when constructing the map on application start
#define SELECT_ALIVE_LEASES "SELECT "\
                                "RtTable,AddrFamily "\
                            "FROM "\
                                "RtTables "\
                            "WHERE "\
                                "LeaseExpires > ?"

struct tas_ctx;
struct tas_client_req;

typedef void (*check_table_cb)(void *ptr, uint8_t addr_family,
        uint32_t rt_table);

//create db and set up queries. Returns 0/1 on failure/success
uint8_t table_allocator_server_sqlite_create_db(struct tas_ctx *ctx);

//insert a table into the database
uint8_t table_allocator_sqlite_insert_table(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t rt_table, time_t lease_sec);

//checks if the current (family, ifname, addr) has a lease and returns the table
//if it is the case
uint32_t table_allocator_sqlite_get_table(struct tas_ctx *ctx,
        struct tas_client_req *req);

//remove one table allocation from the database
uint8_t table_allocator_sqlite_remove_table(struct tas_ctx *ctx,
        struct tas_client_req *req);

//update lease seq
uint8_t table_allocator_sqlite_update_lease(struct tas_ctx *ctx,
        uint32_t rt_table, uint8_t addr_family, uint32_t lease_sec);

//delete dead leases, don't care about family
uint8_t table_allocator_sqlite_delete_dead_leases(struct tas_ctx *ctx,
        uint32_t lease_limit, check_table_cb cb);

//update table map with info from db on start of application
uint8_t table_allocator_sqlite_build_table_map(struct tas_ctx *ctx,
        uint32_t lease_limit, check_table_cb cb);

#endif
