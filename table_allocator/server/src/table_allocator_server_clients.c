#include <stdint.h>

#include <table_allocator_log.h>

#include "table_allocator_server_clients.h"
#include "table_allocator_server.h"

//return 0/1 on success, on successs, table is stored in table
uint8_t table_allocator_server_clients_handle_req(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t *table)
{
    //check database for existing table allocation

    //allocate table if found
    return 0;
}

uint8_t table_allocator_server_clients_handle_release(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t table)
{
    return 0;
}
