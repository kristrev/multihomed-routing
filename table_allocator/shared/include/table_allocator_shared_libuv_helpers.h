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

#ifndef TABLE_ALLOCATOR_SHARED_LIBUV_HELPERS
#define TABLE_ALLOCATOR_SHARED_LIBUV_HELPERS

#include <uv.h>

uint8_t ta_allocator_libuv_helpers_configure_unix_handle(uv_loop_t *event_loop,
        uv_udp_t *unix_socket_handle, uv_timer_t *unix_socket_timeout_handle,
        uv_timer_cb unix_socket_timeout_cb, void *ptr);

#endif
