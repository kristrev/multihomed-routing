#include <uv.h>
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>

#include <table_allocator_shared_log.h>

#include "table_allocator_client_netlink.h"
#include "table_allocator_client.h"

static void table_allocator_client_netlink_alloc_cb(uv_handle_t *handle_size,
        size_t suggested_size, uv_buf_t *buf)
{
    struct tac_ctx *ctx = handle_size->data;

    buf->base = (char*) ctx->mnl_recv_buf;
    buf->len = MNL_SOCKET_BUFFER_SIZE;
}

static void table_allocator_client_netlink_recv_cb(uv_udp_t* handle,
        ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr,
        unsigned flags)
{

}

uint8_t table_allocator_client_netlink_configure(struct tac_ctx *ctx)
{
    struct mnl_socket *mnl_sock = NULL;

    if ((mnl_sock = mnl_socket_open(NETLINK_ROUTE)) == NULL) {
        TA_PRINT(stderr, "Failed to open netlink socket\n");
        return 0;
    }

    //todo: make this depend on which address family is set
    if (mnl_socket_bind(mnl_sock, (1 << (RTNLGRP_IPV4_IFADDR - 1)) |
                (1 << (RTNLGRP_LINK - 1)), MNL_SOCKET_AUTOPID) < 0)
    {
        TA_PRINT(stderr,  "Failed to bind netlink socket\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    ctx->rt_mnl_socket = mnl_sock;

    if (uv_udp_init(&(ctx->event_loop), &(ctx->netlink_handle))) {
        TA_PRINT(stderr, "Intializing netlink udp handle failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    if (uv_udp_open(&(ctx->netlink_handle), mnl_socket_get_fd(mnl_sock))) {
        TA_PRINT(stderr, "Opening netlink udp handle failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    if (uv_udp_recv_start(&(ctx->netlink_handle),
                table_allocator_client_netlink_alloc_cb,
                table_allocator_client_netlink_recv_cb)) {
        TA_PRINT(stderr, "Starting netlink receive failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    ctx->netlink_handle.data = ctx;

    return 1;
}
