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

#ifndef TABLE_ALLOCATOR_CLIENT_NETLINK_H
#define TABLE_ALLOCATOR_CLIENT_NETLINK_H

#define ADDR_RULE_PRIO          10000
#define NW_RULE_PRIO            20000
#define DEF_RULE_PRIO           91000

//how long to wait until we try to add rules again
#define TAC_NETLINK_TIMEOUT_MS  1000

struct nlattr;
struct tac_ctx;

struct nlattr_storage {
    const struct nlattr **tb;
    uint32_t limit;
};

//configure netlink + start listening
uint8_t table_allocator_client_netlink_configure(struct tac_ctx *ctx);

//stop netlink handling
void table_allocator_client_netlink_stop(struct tac_ctx *ctx);

//add rules. If adding rules fails, then we will start a timer
void table_allocator_client_netlink_update_rules(struct tac_ctx *ctx,
        uint32_t msg_type);

#endif
