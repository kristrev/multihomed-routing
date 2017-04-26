#include <stdint.h>
#include <strings.h>

#include <table_allocator_shared_log.h>

#include "table_allocator_server_clients.h"
#include "table_allocator_server.h"

static uint32_t allocate_table(struct tas_ctx *ctx, uint8_t addr_family)
{
    uint32_t rt_table = 0;
    uint32_t *rt_tables = NULL;

    if (addr_family == AF_INET) {
        rt_tables = ctx->tables_inet; 
    } else if (addr_family == AF_INET6) {
        rt_tables = ctx->tables_inet6;
    } else {
        rt_tables = ctx->tables_unspec;
    }

    for (int i = 0; i < MAX_NUM_TABLES; i++) {
        //Zero means all indexes represented by this element is taken
        if(!rt_tables[i])
            continue;

        //Lowest value returned by ffs is 1, so must fix when setting
        rt_table = ffs(rt_tables[i]);
        rt_tables[i] ^= (1 << (rt_table - 1));
        rt_table += (i*(sizeof(rt_tables[i]))*8);
        break;
    }

    //Prevent reurning file descriptors larger than the limit. Larger than
    //because lowest bit has index 1, not 0 (so we have 1-MAX and not 0-MAX -1)
    if (rt_table > MAX_NUM_TABLES)
        return 0;
    else
        return rt_table;
}

static uint8_t release_table(struct tas_ctx *ctx)
{
#if 0
    uint32_t element_index = (ip4table - 1) >> 5;
    //What we do here is to mask out the lowest five bits. They contain the
    //index of the bit to be set (remember that 32 is 0x20);
    int32_t element_bit = (ip4table - 1) & 0x1F;

    mIp4TableVals[element_index] ^= (1 << element_bit);
#endif
    return 0;
}

//return 0/1 on success, on successs, table is stored in table. Reason for not
//just returning table, is that we might want to expand with more error codes
//later
uint8_t table_allocator_server_clients_handle_req(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t *rt_table)
{
    uint32_t rt_table_returned = 0;
    //check database for existing table allocation

    //allocate table if not foundfound
    if (!(rt_table_returned = allocate_table(ctx, req->addr_family)))
        return 0;

    TA_PRINT(ctx->logfile, "Allocated table %u\n", rt_table_returned);

    *rt_table = rt_table_returned;
    return 1;
}

uint8_t table_allocator_server_clients_handle_release(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t table)
{
    return 0;
}
