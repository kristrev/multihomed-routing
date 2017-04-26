#include <string.h>
#include <unistd.h>

#include "table_allocator_server_sockets.h"
#include "table_allocator_server.h"

#include "table_allocator_log.h"
#include "table_allocator_socket_helpers.h"

static void unix_socket_alloc_cb(uv_handle_t* handle, size_t suggested_size,
            uv_buf_t* buf)
{
    struct tas_ctx *ctx = handle->data;

    buf->base = (char*) ctx->client_req_buffer;
    buf->len = sizeof(ctx->client_req_buffer);
}

static void unix_socket_handle_close_cb(uv_handle_t *handle)
{
    struct tas_ctx *ctx = handle->data;

    uv_udp_init(&(ctx->event_loop), &(ctx->unix_socket_handle));
    uv_timer_start(&(ctx->unix_socket_timeout_handle), unix_socket_timeout_cb,
            0, 0);
}

static void unix_socket_stop_recv(struct tas_ctx *ctx)
{
    uv_udp_recv_stop(&(ctx->unix_socket_handle));

    //in case we get called multiple times, for example from event cache
	if (!uv_is_closing((uv_handle_t*) & (ctx->unix_socket_handle)))
		uv_close((uv_handle_t*) &(ctx->unix_socket_handle),
                unix_socket_handle_close_cb);
}

static void unix_socket_recv_cb(uv_udp_t* handle, ssize_t nread,
        const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
    struct tas_ctx *ctx = handle->data;
    int32_t retval, sock_fd;

    if (nread == 0) {
        return;
    } else if (nread < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_DEBUG, "Server socket failed, error: %s\n",
                uv_strerror(nread));
        unix_socket_stop_recv(ctx);
        return;
    }

    //handle failure on socket. Probably not needed for domain sockets, but add
    //it in case we at some point want to for example add support for sending
    //messages through the network

#if 0
    uv_fileno((const uv_handle_t*) handle, &sock_fd);

    TA_PRINT_SYSLOG(ctx, LOG_DEBUG, "Received %zd bytes\n", nread);

    //addr is always null, client needs to bind and pass the address in the JSON
    retval = sendto(sock_fd, buf->base, nread, 0, (const struct sockaddr*) addr,
            sizeof(struct sockaddr_un));

    if (retval < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Sending error: %s\n", strerror(errno));
    } else {
        TA_PRINT(ctx->logfile, "Sent %d bytes\n", retval);

    }
#endif
}

void unix_socket_timeout_cb(uv_timer_t *handle)
{
    int32_t sock_fd = -1, retval;
    uint8_t success = 1;
    struct tas_ctx *ctx = handle->data;

    if (uv_fileno((const uv_handle_t*) &(ctx->unix_socket_handle), &sock_fd)
            == UV_EBADF) {
        //path will be read from config, stored in ctx
        sock_fd = ta_socket_helpers_create_unix_socket("test-test-test");

        if (sock_fd < 0 || uv_udp_open(&(ctx->unix_socket_handle), sock_fd)) {
            //print error
            if (sock_fd < 0) {
                TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to create domain socket: "
                        "%s\n", strerror(errno));
                //log error from errno 
            } else {
                close(sock_fd);
            }

            success = 0;
        }
    }

    if (success) {
        retval = uv_udp_recv_start(&(ctx->unix_socket_handle),
                unix_socket_alloc_cb, unix_socket_recv_cb);

        if (retval < 0) {
            TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to start domain socket: "
                            "%s\n", uv_strerror(retval));
            success = 0;
            //reset handle, since we have already attached a socket to it
            //todo: if this fails, stop loop
            uv_udp_init(&(ctx->event_loop), &(ctx->unix_socket_handle));
            close(sock_fd);
        }
    }

    if (success) {
	    uv_timer_stop(handle);
    } else {
        if (!uv_timer_get_repeat(handle)) {
		    uv_timer_set_repeat(handle, DOMAIN_SOCKET_TIMEOUT_MS);
			uv_timer_again(handle);
		}
    }
}

