#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <uv.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>

#include "table_allocator_client.h"
#include "table_allocator_log.h"
#include "table_allocator_libuv_helpers.h"
#include "table_allocator_socket_helpers.h"

static void table_allocator_client_send_request(struct tac_ctx *ctx);

static void unix_socket_alloc_cb(uv_handle_t* handle, size_t suggested_size,
            uv_buf_t* buf)
{

}

static void unix_socket_recv_cb(uv_udp_t* handle, ssize_t nread,
        const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{

}

static void client_request_timeout_handle(uv_timer_t *handle)
{
    struct tac_ctx *ctx = handle->data;

    table_allocator_client_send_request(ctx);
}

static void table_allocator_client_send_request(struct tac_ctx *ctx)
{
    uint8_t snd_buf[1] = {'a'};
	struct sockaddr_un remote_addr;
    //this is just a test until we have config files in place
    const char *path_name = "test-test-test";
    int32_t retval = -1, sock_fd;

    memset(&remote_addr, 0, sizeof(remote_addr));

    remote_addr.sun_family = AF_UNIX;

    //we use abstract naming, so first byte of path is always \0
    strncpy(remote_addr.sun_path + 1, path_name, strlen(path_name));
	uv_fileno((const uv_handle_t*) &(ctx->unix_socket_handle), &sock_fd);

    //todo: I need to use sendto, it seems the diffrent send-methods in libuv
    //expects something that is either sockaddr_in or sockaddr_in6 (investigate)
    retval = sendto(sock_fd, snd_buf, 1, 0, (const struct sockaddr*) &remote_addr, sizeof(struct sockaddr_un));

    if (retval < 0 && retval != UV_EAGAIN) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Sending error: %s\n", uv_strerror(retval));
    } else {
        TA_PRINT(ctx->logfile, "Sent %d bytes\n", retval);

        //todo: check for failure here?
        uv_udp_recv_start(&(ctx->unix_socket_handle), unix_socket_alloc_cb,
                unix_socket_recv_cb);
    }

    //start retransmission timer
    if (uv_timer_start(&(ctx->request_timeout_handle),
                client_request_timeout_handle, REQUEST_RETRANSMISSION_MS, 0)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Can't start request timer\n");
        //do clean-up at least
		exit(EXIT_FAILURE);
	}
}

//todo: consider making shared
static void unix_socket_timeout_cb(uv_timer_t *handle)
{
    int32_t sock_fd = -1;
    uint8_t success = 1;
    struct tac_ctx *ctx = handle->data;

    if (uv_fileno((const uv_handle_t*) &(ctx->unix_socket_handle), &sock_fd)
            == UV_EBADF) {
        //path will be read from config, stored in ctx
        sock_fd = ta_socket_helpers_create_unix_socket(NULL);

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
	    uv_timer_stop(handle);
        table_allocator_client_send_request(ctx);
    } else {
        if (!uv_timer_get_repeat(handle)) {
		    uv_timer_set_repeat(handle, DOMAIN_SOCKET_TIMEOUT_MS);
			uv_timer_again(handle);
		}
    }
}

int main(int argc, char *argv[])
{
    struct tac_ctx *ctx;

    //parse command line arguments

    //create the application context
    ctx = calloc(sizeof(struct tac_ctx), 1);

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
    /*if (!parse_config(ctx, NULL)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Option parsing failed\n");    
        exit(EXIT_FAILURE);
    }*/

    if (!ta_allocator_libuv_helpers_configure_unix_handle(&(ctx->event_loop),
                &(ctx->unix_socket_handle), &(ctx->unix_socket_timeout_handle),
                unix_socket_timeout_cb, ctx)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Failed to configure domain handle\n");
        exit(EXIT_FAILURE);
    }
    
    if (uv_timer_init(&(ctx->event_loop), &(ctx->request_timeout_handle))) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Failed to init request timeout\n");
		exit(EXIT_FAILURE);
	}

    ctx->request_timeout_handle.data = ctx;

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Started allocator client\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);

    //clean up allocated memory

    exit(EXIT_SUCCESS);
}
