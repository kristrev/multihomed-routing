#ifndef TABLE_ALLOCATOR_SERVER_CLIENTS_H
#define TABLE_ALLOCATOR_SERVER_CLIENTS_H

struct tas_ctx;
struct tas_client_req;

uint8_t table_allocator_server_clients_handle_req(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t *rt_table);

uint8_t table_allocator_server_clients_handle_release(struct tas_ctx *ctx,
        struct tas_client_req *req);

#endif
