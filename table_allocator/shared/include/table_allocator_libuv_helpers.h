#ifndef TABLE_ALLOCATOR_LIBUV_HELPERS
#define TABLE_ALLOCATOR_LIBUV_HELPERS

#include <uv.h>

uint8_t ta_allocator_libuv_helpers_configure_unix_handle(uv_loop_t *event_loop,
        uv_udp_t *unix_socket_handle, uv_timer_t *unix_socket_timeout_handle,
        uv_timer_cb unix_socket_timeout_cb, void *ptr);

#endif
