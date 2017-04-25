#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <uv.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <getopt.h>
#include <arpa/inet.h>

#include "table_allocator_client.h"
#include "table_allocator_log.h"
#include "table_allocator_libuv_helpers.h"
#include "table_allocator_socket_helpers.h"

static void table_allocator_client_send_request(struct tac_ctx *ctx);
static void unix_socket_timeout_cb(uv_timer_t *handle);

static void unix_socket_handle_close_cb(uv_handle_t *handle)
{
    struct tac_ctx *ctx = handle->data;

    //todo: error checks
    uv_udp_init(&(ctx->event_loop), &(ctx->unix_socket_handle));
    uv_timer_start(&(ctx->unix_socket_timeout_handle), unix_socket_timeout_cb,
            0, 0);
}

static void unix_socket_alloc_cb(uv_handle_t* handle, size_t suggested_size,
            uv_buf_t* buf)
{

}

static void unix_socket_recv_cb(uv_udp_t* handle, ssize_t nread,
        const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{

}

static void client_request_timeout_handle_cb(uv_timer_t *handle)
{
    struct tac_ctx *ctx = handle->data;

    table_allocator_client_send_request(ctx);

#if 0
	uv_udp_recv_stop(&(ctx->unix_socket_handle));
	uv_timer_stop(&(ctx->request_timeout_handle));
	uv_timer_stop(&(ctx->unix_socket_timeout_handle));

	if (!uv_is_closing((uv_handle_t*) & (ctx->unix_socket_handle)))
		uv_close((uv_handle_t*) &(ctx->unix_socket_handle),
                unix_socket_handle_close_cb);
#endif
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
                client_request_timeout_handle_cb, REQUEST_RETRANSMISSION_MS, 0)) {
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

static void free_ctx(struct tac_ctx *ctx)
{

	uv_loop_close(&(ctx->event_loop));
	free(ctx);
}

static void usage()
{
    fprintf(stdout, "Usage: table_allocator_client [options ...]\n");
    fprintf(stdout, "Required arguments are marked with <r>\n");
    fprintf(stdout, "\t-4: set address family to IPv4 (default is UNSPEC)\n");
    fprintf(stdout, "\t-6: set address family to IPv6\n");
    fprintf(stdout, "\t-s/--syslog: enable logging to syslog (default off)\n");
    fprintf(stdout, "\t-l/--log_path: path to logfile (default stderr)\n");
    fprintf(stdout, "\t-a/--address: address to allocate table for <r>\n");
    fprintf(stdout, "\t-i/--iface: interface to allocate table for <r>\n");
    fprintf(stdout, "\t-t/--tag: optional tag to send to server\n");
    fprintf(stdout, "\t-h/--help: this information\n");
}

static uint8_t parse_cmd_args(struct tac_ctx *ctx, int argc, char *argv[])
{
    int32_t option_index, opt;
    const char *address = NULL, *ifname = NULL, *log_path = NULL, *tag = NULL;
    const char *destination = NULL;
    struct sockaddr_storage addr_tmp;
    struct option options[] = {
        {"syslog", no_argument, NULL, 's'},
        {"log_path", required_argument, NULL, 'l'},
        {"address", required_argument, NULL, 'a'},
        {"iface", required_argument, NULL, 'i'},
        {"tag", required_argument, NULL, 't'},
        {"destination", required_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
    };

    while (1) {
        opt = getopt_long(argc, argv, "46sl:a:i:t:d:h", options, &option_index);

        if (opt == -1)
            break;
        
        switch (opt) {
        case '4':
            ctx->addr_family = AF_INET;
            break;
        case '6':
            ctx->addr_family = AF_INET6;
            break;
        case 's':
            ctx->use_syslog = 1;
            break;
        case 'l':
            log_path = optarg;
            break;
        case 'a':
            address = optarg;
            break;
        case 'i':
            ifname = optarg;
            break;
        case 't':
            tag = optarg;
            break;
        case 'd':
            destination = optarg;
            break;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (address == NULL || ifname == NULL || destination == NULL) {
        fprintf(stderr, "Missing required argument\n");
        usage();
        return 0;
    }

    if (strlen(tag) >= MAX_TAG_SIZE) {
        fprintf(stderr, "Tag name too long (%zd > %u\n", strlen(tag),
                MAX_TAG_SIZE);
        return 0;
    } else {
        memcpy(ctx->tag, tag, strlen(tag));
    }

    if (strlen(ifname) >= IFNAMSIZ) {
        fprintf(stderr, "Interface name too long (%zd > %u\n", strlen(ifname),
                IFNAMSIZ);
        return 0;
    } else {
        memcpy(ctx->ifname, ifname, strlen(ifname));
    }

    if (strlen(destination) >= MAX_ADDR_SIZE) {
        fprintf(stderr, "Destination too long (%zd > %u\n", strlen(destination),
                MAX_ADDR_SIZE);
        return 0;
    } else {
        memcpy(ctx->destination, destination, strlen(destination));
    }

    if (ctx->addr_family == AF_INET) {
        if (!inet_pton(AF_INET, address, &addr_tmp)) {
            fprintf(stderr, "Address is not valid: %s\n", address);
            return 0;
        } else {
            memcpy(ctx->destination, destination, strlen(destination));
        }
    } else if (ctx->addr_family == AF_INET6) {
        if (!inet_pton(AF_INET6, address, &addr_tmp)) {
            fprintf(stderr, "Address is not valid: %s\n", address);
            return 0;
        } else {
            memcpy(ctx->destination, destination, strlen(destination));
        }
    } else {
        if (strlen(address) >= MAX_ADDR_SIZE) {
            fprintf(stderr, "Address too long (%zd > %u\n", strlen(address),
                    MAX_ADDR_SIZE);
            return 0;
        } else {
            memcpy(ctx->destination, destination, strlen(destination));
        }
    }

    if (log_path) {
        if (!(ctx->logfile = fopen(log_path, "w"))) {
            fprintf(stderr, "Could not open file: %s (%s)\n", log_path,
                    strerror(errno));
            return 0;
        }
    } 

    return 1;
}

int main(int argc, char *argv[])
{
    struct tac_ctx *ctx;

    //parse command line arguments

    //create the application context
    ctx = calloc(sizeof(struct tac_ctx), 1);

    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        exit(EXIT_FAILURE);
    }
 
    //set default values for context
    ctx->use_syslog = 0;
    ctx->logfile = stderr;
    ctx->addr_family = AF_UNSPEC;

    if (!parse_cmd_args(ctx, argc, argv)) {
        free(ctx);
        exit(EXIT_FAILURE);
    }

    //create event loop
    if (uv_loop_init(&(ctx->event_loop))) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Event loop creation failed\n");
        free_ctx(ctx);
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
        free_ctx(ctx);
        exit(EXIT_FAILURE);
    }
    
    if (uv_timer_init(&(ctx->event_loop), &(ctx->request_timeout_handle))) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Failed to init request timeout\n");
        free_ctx(ctx);
		exit(EXIT_FAILURE);
	}

    ctx->request_timeout_handle.data = ctx;

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Started allocator client\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);
    free_ctx(ctx);

    exit(EXIT_SUCCESS);
}
