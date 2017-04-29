#ifndef TABLE_ALLOCATOR_SERVER_CLIENTS_H
#define TABLE_ALLOCATOR_SERVER_CLIENTS_H

struct tas_ctx;
struct tas_client_req;

uint8_t table_allocator_server_clients_handle_req(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t *rt_table,
        uint32_t *lease_sec_ptr);

uint8_t table_allocator_server_clients_handle_release(struct tas_ctx *ctx,
        struct tas_client_req *req);

void table_allocator_server_clients_delete_dead_leases(struct tas_ctx *ctx);

void table_allocator_server_clients_set_table(struct tas_ctx *ctx,
        uint8_t addr_family, uint32_t rt_table);
#endif
