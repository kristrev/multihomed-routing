#include <string.h>
#include <unistd.h>
#include <sys/un.h>

#include "table_allocator_server_sockets.h"
#include "table_allocator_server.h"
#include "table_allocator_server_clients.h"

#include <table_allocator_shared_log.h>
#include <table_allocator_shared_socket_helpers.h>

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
    struct tas_client_req *req = ctx->req;
    int32_t retval, sock_fd;
    uint32_t table, reply_buf_len = 0;
    uint8_t reply_buf[TA_SHARED_MAX_JSON_LEN] = {0};
    const struct sockaddr_un *un_addr = (const struct sockaddr_un*) addr;

    //ignore data done or partial reads, or buffer is big enough to store all
    //messages we send
    if (nread == 0 || flags & UV_UDP_PARTIAL) {
        return;
    } else if (nread < 0) {
        //failure handling is not really needed for domain sockets, once created
        //they can't really fail. But it add so that it is in place in case we
        //ever switch/add support for a different socket type (and it never
        //hurts)
        TA_PRINT_SYSLOG(ctx, LOG_DEBUG, "Server socket failed, error: %s\n",
                uv_strerror(nread));
        unix_socket_stop_recv(ctx);
        return;
    }

    memset(ctx->req, 0, sizeof(struct tas_client_req));

    //parse json
    if (!table_allocator_shared_json_parse_seq(buf->base, &(req->addr_family),
                &(req->cmd), &(req->ver), req->address, req->ifname,
                req->tag)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to parse request from: %s\n",
                un_addr->sun_path + 1);
        return;
    }

    TA_PRINT(ctx->logfile, "Parsed request from %s\n", un_addr->sun_path + 1);

    //check command and release/request table
    if (req->cmd == TA_SHARED_CMD_REQ) {
        if (table_allocator_server_clients_handle_req(ctx, req, &table)) {
            reply_buf_len = table_allocator_shared_json_gen_response(table,
                    reply_buf);
            //return response
        } else {
            //return generic error response
        }

    } else if (req->cmd == TA_SHARED_CMD_REL) {
    
    } else {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Received unknown command %u from %s\n",
                req->cmd, un_addr->sun_path);
        return;
    }

    if (!reply_buf_len) {
        return;
    }

    TA_PRINT_SYSLOG(ctx, LOG_DEBUG, "Will send %s to client %s\n", reply_buf,
            un_addr->sun_path);

    //send reply
    uv_fileno((const uv_handle_t*) handle, &sock_fd);

    //addr is always null, client needs to bind and pass the address in the JSON
    retval = sendto(sock_fd, reply_buf, reply_buf_len, 0, addr,
            sizeof(struct sockaddr_un));

    if (retval < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Sending error: %s\n", strerror(errno));
    } else {
        TA_PRINT(ctx->logfile, "Sent %d bytes\n", retval);

    }
}

void unix_socket_timeout_cb(uv_timer_t *handle)
{
    int32_t sock_fd = -1, retval;
    uint8_t success = 1;
    struct tas_ctx *ctx = handle->data;

    if (uv_fileno((const uv_handle_t*) &(ctx->unix_socket_handle), &sock_fd)
            == UV_EBADF) {
        //path will be read from config, stored in ctx
        sock_fd = ta_socket_helpers_create_unix_socket((const char *)
                ctx->socket_path);

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

