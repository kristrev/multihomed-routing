#ifndef TABLE_ALLOCATOR_SERVER_SOCKETS_H
#define TABLE_ALLOCATOR_SERVER_SOCKETS_H

#include <uv.h>

void unix_socket_timeout_cb(uv_timer_t *handle);

#endif
