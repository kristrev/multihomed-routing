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
#include <time.h>
#include <net/if.h>

#include "table_allocator_client.h"
#include "table_allocator_client_netlink.h"

#include <table_allocator_shared_json.h>
#include <table_allocator_shared_log.h>
#include <table_allocator_shared_libuv_helpers.h>
#include <table_allocator_shared_socket_helpers.h>

static void table_allocator_client_send_request(struct tac_ctx *ctx);
static void unix_socket_timeout_cb(uv_timer_t *handle);
static void client_request_timeout_handle_cb(uv_timer_t *handle);

static void unix_socket_handle_close_cb(uv_handle_t *handle)
{
    struct tac_ctx *ctx = handle->data;

    //todo: error checks
    uv_udp_init(&(ctx->event_loop), &(ctx->unix_socket_handle));
    uv_timer_start(&(ctx->unix_socket_timeout_handle), unix_socket_timeout_cb,
            0, 0);
}

static void unix_socket_stop_recv(struct tac_ctx *ctx)
{
	uv_udp_recv_stop(&(ctx->unix_socket_handle));
	uv_timer_stop(&(ctx->request_timeout_handle));
	uv_timer_stop(&(ctx->unix_socket_timeout_handle));

	if (!uv_is_closing((uv_handle_t*) & (ctx->unix_socket_handle)))
		uv_close((uv_handle_t*) &(ctx->unix_socket_handle),
                unix_socket_handle_close_cb);
}

static void unix_socket_alloc_cb(uv_handle_t* handle, size_t suggested_size,
            uv_buf_t* buf)
{
    struct tac_ctx *ctx = handle->data;

    buf->base = (char*) ctx->rcv_buf;
    buf->len = TA_SHARED_MAX_JSON_LEN;
}

static void unix_socket_recv_cb(uv_udp_t* handle, ssize_t nread,
        const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
    struct tac_ctx *ctx = handle->data;
    struct tac_address *address = ctx->address;
    uint8_t ver, cmd;
    const struct sockaddr_un *un_addr = (const struct sockaddr_un*) addr;
    struct timespec tv_now;
    uint32_t tdiff;

    //rearm send timer
    //todo: look at error handling here, if I have missed something
    if (nread == 0) {
        return;
    } else if (flags & UV_UDP_PARTIAL) {
        uv_timer_start(&(ctx->request_timeout_handle),
                client_request_timeout_handle_cb, REQUEST_RETRANSMISSION_MS, 0);
        return;
    } else if (nread < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_DEBUG, "Client socket failed, error: %s\n",
                uv_strerror(nread));
        unix_socket_stop_recv(ctx);
        return;
    }

    //parse json
    if (!tables_allocator_shared_json_parse_client_reply(buf->base,
                &ver, &cmd, &(address->rt_table), &(address->lease_expires))) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to parse request from: %s\n",
                un_addr->sun_path + 1);
        return;
    }


    //check cmd and version
    if (cmd != TA_SHARED_CMD_RESP || ver != TA_VERSION ||
            !(address->rt_table)) {
        uv_timer_start(&(ctx->request_timeout_handle),
                client_request_timeout_handle_cb, REQUEST_RETRANSMISSION_MS, 0);
        return;
    }

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Server %s Table %u Lease %u\n",
            un_addr->sun_path + 1, address->rt_table, address->lease_expires);

    clock_gettime(CLOCK_MONOTONIC_RAW, &tv_now);
    //todo: guard this better
    tdiff = address->lease_expires - tv_now.tv_sec;

    //write table
    printf("%u\n", address->rt_table);
    fflush(stdout);

    //start running as daemon
    //todo: add some error handling or just fail?
    if (!ctx->daemonized && ctx->daemonize && daemon(0, 0)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Failed to daemonize client");
        exit(EXIT_FAILURE);
    }

    ctx->daemonized = 1;
    //start request timeout again
    uv_timer_start(&(ctx->request_timeout_handle),
            client_request_timeout_handle_cb, (tdiff/2)*1000, 0);
}

static void client_request_timeout_handle_cb(uv_timer_t *handle)
{
    struct tac_ctx *ctx = handle->data;
    table_allocator_client_send_request(ctx);
}

static void table_allocator_client_send_request(struct tac_ctx *ctx)
{
    struct tac_address *address = ctx->address;
	struct sockaddr_un remote_addr;
    //this is just a test until we have config files in place
    int32_t retval = -1, sock_fd;
    struct json_object *req_obj = NULL;
    const char *json_str;
    
    req_obj = table_allocator_shared_json_create_req(address->address_str,
            address->ifname, address->tag, address->addr_family, ctx->cmd);

    if (!req_obj) {
        if (uv_timer_start(&(ctx->request_timeout_handle),
                    client_request_timeout_handle_cb, REQUEST_RETRANSMISSION_MS,
                    0)) {
            TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Can't start request timer\n");
		    exit(EXIT_FAILURE);
	    }

        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to create request json for "
                "%s-%s-%u\n", address->ifname, address->address_str,
                address->addr_family);
        return;
    }

    json_str = json_object_to_json_string_ext(req_obj, JSON_C_TO_STRING_PLAIN);

    TA_PRINT_SYSLOG(ctx, LOG_DEBUG, "JSON request: %s %zd\n",
            json_str, strlen(json_str));

    //populate address
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sun_family = AF_UNIX;

    //we use abstract naming, so first byte of path is always \0
    strncpy(remote_addr.sun_path + 1, ctx->destination,
            strlen(ctx->destination));
	uv_fileno((const uv_handle_t*) &(ctx->unix_socket_handle), &sock_fd);

    //todo: I need to use sendto, it seems the diffrent send-methods in libuv
    //expects something that is either sockaddr_in or sockaddr_in6 (investigate)
    retval = sendto(sock_fd, json_str, strlen(json_str), 0,
            (const struct sockaddr*) &remote_addr, sizeof(struct sockaddr_un));
    json_object_put(req_obj);

    if (retval < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Sending error: %s\n", uv_strerror(retval));
    } else {
        TA_PRINT(ctx->logfile, "Sent %d bytes\n", retval);

        //todo: check for failure here?
        uv_udp_recv_start(&(ctx->unix_socket_handle), unix_socket_alloc_cb,
                unix_socket_recv_cb);
    }

    //always start retransmission timer, independent of success of failure
    //timer will be updated in the different handler functions
    if (uv_timer_start(&(ctx->request_timeout_handle),
                client_request_timeout_handle_cb, REQUEST_RETRANSMISSION_MS,
                0)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Can't start request timer\n");
		exit(EXIT_FAILURE);
	}
}

//todo: consider making shared
static void unix_socket_timeout_cb(uv_timer_t *handle)
{
    int32_t sock_fd = -1;
    uint8_t success = 1;
    struct tac_ctx *ctx = handle->data;
    struct tac_address *address = ctx->address;
    //format is ifname-addr-family. The space for the two "-" comes for free via
    //the additional byte in IFNAMSIZE and INET6_ADDRSTRLEN, family is maximum
    //two digits (IPv6 is 10)
    char unix_socket_addr[IFNAMSIZ + INET6_ADDRSTRLEN + 3];

    if (uv_fileno((const uv_handle_t*) &(ctx->unix_socket_handle), &sock_fd)
            == UV_EBADF) {
        snprintf(unix_socket_addr, sizeof(unix_socket_addr),
                "%s-%s-%u", address->ifname, address->address_str,
                address->addr_family);

        //path will be read from config, stored in ctx
        sock_fd = ta_socket_helpers_create_unix_socket(unix_socket_addr);

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
    if (ctx->rt_mnl_socket) {
        table_allocator_client_netlink_stop(ctx);
    }

	uv_loop_close(&(ctx->event_loop));

    free(ctx->mnl_recv_buf);
    free(ctx->address);
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
    fprintf(stdout, "\t-n/--netmask: netmask for use with address <r>\n");
    fprintf(stdout, "\t-i/--ifname: interface to allocate table for <r>\n");
    fprintf(stdout, "\t-t/--tag: optional tag to send to server\n");
    fprintf(stdout, "\t-r/--release: set command to release instead of request\n");
    fprintf(stdout, "\t-d/--destination: Path to server socket <r>\n");
    fprintf(stdout, "\t-f/--foreground: Run application in foreground\n");
    fprintf(stdout, "\t-h/--help: this information\n");
}

static uint8_t compute_prefix_len_4(struct in_addr *netmask)
{
    return 32 - (ffs(ntohl(netmask->s_addr)) - 1);
}

static uint8_t parse_cmd_args(struct tac_ctx *ctx, int argc, char *argv[])
{
    int32_t option_index, opt;
    const char *address = NULL, *ifname = NULL, *log_path = NULL, *tag = NULL;
    const char *destination = NULL, *netmask = NULL;

    union {
        struct sockaddr_in *addr4;
        struct sockaddr_in6 *addr6;
    } u_addr;

    union {
        struct in_addr netmask4;
        struct in6_addr netmask6;
    } u_netmask;

    struct option options[] = {
        {"syslog", no_argument, NULL, 's'},
        {"log_path", required_argument, NULL, 'l'},
        {"address", required_argument, NULL, 'a'},
        {"netmask", required_argument, NULL, 'n'},
        {"ifname", required_argument, NULL, 'i'},
        {"tag", required_argument, NULL, 't'},
        {"release", no_argument, NULL, 'r'},
        {"destination", required_argument, NULL, 'd'},
        {"foreground", required_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
    };

    while (1) {
        opt = getopt_long(argc, argv, "46sl:a:n:i:t:d:r:fh", options, &option_index);

        if (opt == -1)
            break;
        
        switch (opt) {
        case '4':
            ctx->address->addr_family = AF_INET;
            break;
        case '6':
            ctx->address->addr_family = AF_INET6;
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
        case 'n':
            netmask = optarg;
            break;
        case 'i':
            ifname = optarg;
            break;
        case 't':
            tag = optarg;
            break;
        case 'r':
            ctx->cmd = TA_SHARED_CMD_REL;
            break;
        case 'd':
            destination = optarg;
            break;
        case 'f':
            ctx->daemonize = 0;
            break;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (!address || (ctx->address->addr_family == AF_INET && !netmask) ||
            !ifname || !destination) {
        fprintf(stderr, "Missing required argument\n");
        usage();
        return 0;
    }

    if (tag) {
        if (strlen(tag) >= TA_SHARED_MAX_TAG_SIZE) {
            fprintf(stderr, "Tag name too long (%zd > %u)\n", strlen(tag),
                    TA_SHARED_MAX_TAG_SIZE - 1);
            return 0;
        } else {
            memcpy(ctx->address->tag, tag, strlen(tag));
        }
    }

    if (strlen(ifname) >= IFNAMSIZ) {
        fprintf(stderr, "Interface name too long (%zd > %u)\n", strlen(ifname),
                IFNAMSIZ - 1);
        return 0;
    } else {
        memcpy(ctx->address->ifname, ifname, strlen(ifname));
    }

    if (strlen(destination) >= TA_SHARED_MAX_ADDR_SIZE) {
        fprintf(stderr, "Destination too long (%zd > %u)\n", strlen(destination),
                TA_SHARED_MAX_ADDR_SIZE - 1);
        return 0;
    } else {
        memcpy(ctx->destination, destination, strlen(destination));
    }

    if (ctx->address->addr_family == AF_INET) {
        u_addr.addr4 = (struct sockaddr_in*) &(ctx->address->addr);
        if (!inet_pton(AF_INET, address, &(u_addr.addr4->sin_addr))) {
            fprintf(stderr, "Address is not valid: %s\n", address);
            return 0;
        }

        if (!inet_pton(AF_INET, netmask, &(u_netmask.netmask4))) {
            fprintf(stderr, "Netmask is not valid: %s\n", netmask);
            return 0;
        }

        ctx->address->subnet_prefix_len =
            compute_prefix_len_4(&(u_netmask.netmask4));

        //keep both string and sockaddr around, since we use the string when
        //sending requests
        memcpy(ctx->address->address_str, address, strlen(address));
    } else if (ctx->address->addr_family == AF_INET6) {
        u_addr.addr6 = (struct sockaddr_in6*) &(ctx->address->addr);
        if (!inet_pton(AF_INET6, address, &(u_addr.addr6->sin6_addr))) {
            fprintf(stderr, "Address is not valid: %s\n", address);
            return 0;
        }

        //todo: find out how prefix len is passed to dhcp
        memcpy(ctx->address->address_str, address, strlen(address));
    } else {
        if (strlen(address) >= INET6_ADDRSTRLEN) {
            fprintf(stderr, "Address too long (%zd > %u)\n", strlen(address),
                    INET6_ADDRSTRLEN - 1);
            return 0;
        } else {
            memcpy(ctx->address->address_str, address, strlen(address));
        }
    }

    if (log_path) {
        if (!(ctx->logfile = fopen(log_path, "w"))) {
            fprintf(stderr, "Could not open file: %s (%s)\n", log_path,
                    strerror(errno));
            return 0;
        }
    } 

    if (!(ctx->address->ifidx = if_nametoindex(ctx->address->ifname))) {
        fprintf(stderr, "Could not get interface index: %s (%s)\n",
                strerror(errno), ctx->address->ifname);
        return 0;
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
        TA_PRINT(stderr, "Allocating memory for context failed\n");
        exit(EXIT_FAILURE);
    }

    ctx->mnl_recv_buf = calloc(MNL_SOCKET_BUFFER_SIZE, 1);

    if (!ctx->mnl_recv_buf) {
        TA_PRINT(stderr, "Allocating memory for mnl recv buf failed\n");
        exit(EXIT_FAILURE);
    }

    ctx->address = calloc(sizeof(struct tac_address), 1);

    if (!ctx->address) {
        TA_PRINT(stderr, "Allocating memory for address failed\n");
        exit(EXIT_FAILURE);
    }

    //set default values for context
    ctx->use_syslog = 0;
    ctx->logfile = stderr;
    ctx->address->addr_family = AF_UNSPEC;
    ctx->cmd = TA_SHARED_CMD_REQ;
    ctx->daemonize = 1;

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

    if (ctx->address->addr_family == AF_INET &&
            !table_allocator_client_netlink_configure(ctx)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Netlink init failed\n");
        free_ctx(ctx);
		exit(EXIT_FAILURE);
    }

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Started allocator client\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);
    free_ctx(ctx);

    exit(EXIT_SUCCESS);
}
