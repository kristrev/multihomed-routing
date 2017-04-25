#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "table_allocator_server.h"

#include "table_allocator_log.h"
#include "table_allocator_socket_helpers.h"
#include "table_allocator_libuv_helpers.h"

static void unix_socket_alloc_cb(uv_handle_t* handle, size_t suggested_size,
            uv_buf_t* buf)
{

}

static void unix_socket_recv_cb(uv_udp_t* handle, ssize_t nread,
        const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{

}

static void unix_socket_timeout_cb(uv_timer_t *handle)
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

static uint8_t parse_config(struct tas_ctx *ctx, const char *conf_path)
{
    return 1;
}

int main(int argc, char *argv[])
{
    struct tas_ctx *ctx;

    //create the application context
    ctx = calloc(sizeof(struct tas_ctx), 1);

    if (!ctx) {
        TA_PRINT(stderr, "Failed to create context\n");
        exit(EXIT_FAILURE);
    }
 
    //parse options, only syslog and config file to be provided
    ctx->use_syslog = 1;

    //set logfile here so that I can use the syslog macro, will be overridden by
    //parse_config (if a config file is provided)
    ctx->logfile = stderr;

    //create event loop
    if (uv_loop_init(&(ctx->event_loop))) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Event loop creation failed\n");
        exit(EXIT_FAILURE);
    }

    //parse config
    if (!parse_config(ctx, NULL)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Option parsing failed\n");    
        exit(EXIT_FAILURE);
    }
  
    if (!ta_allocator_libuv_helpers_configure_unix_handle(&(ctx->event_loop),
                &(ctx->unix_socket_handle), &(ctx->unix_socket_timeout_handle),
                unix_socket_timeout_cb, ctx)) {
       TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Failed to configure domain handle\n");
        exit(EXIT_FAILURE);
    }

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Ready to start allocator\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);

    //clean up allocated memory

    exit(EXIT_SUCCESS);
}
