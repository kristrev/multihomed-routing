#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "table_allocator_server.h"
#include "table_allocator_server_sockets.h"

#include <table_allocator_shared_log.h>
#include <table_allocator_shared_libuv_helpers.h>

static uint8_t configure_rt_tables(struct tas_ctx *ctx, uint32_t max_num_elems)
{
    uint32_t num_table_elements = 0, i;

    //comput the number of entries we need in the table, done by diving the
    //maximum number of elements on 32
    if (max_num_elems < 32) {
        num_table_elements = 0;
    } else if (max_num_elems & 0x1F) {
        //If number of values is not divisible by 32, I need an additional
        //element to store the remainders. Divisibility is checked by masking
        //with 31. If any bit is set, then value is not divisible by 32
        num_table_elements = (max_num_elems >> 5) + 1;
    } else {
        num_table_elements = max_num_elems >> 5;
    }

    if (!(ctx->tables_inet = calloc(sizeof(uint32_t) * num_table_elements,
                1))) {
        TA_PRINT(stderr, "Failed to allocate v4-tables\n");
        return 0;
    }

    if (!(ctx->tables_inet6 = calloc(sizeof(uint32_t) * num_table_elements,
                1))) {
        TA_PRINT(stderr, "Failed to allocate v6-tables\n");
        return 0;
    }

    if (!(ctx->tables_unspec = calloc(sizeof(uint32_t) * num_table_elements,
                1))) {
        TA_PRINT(stderr, "Failed to allocate unspec-tables\n");
        return 0;
    }

    //set all tables as free
    for (i = 0; i < num_table_elements; i++) {
        ctx->tables_inet[i] = UINT32_MAX;
        ctx->tables_inet6[i] = UINT32_MAX;
        ctx->tables_unspec[i] = UINT32_MAX;
    }

    ctx->num_table_elements = num_table_elements;

    return 1;
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
        TA_PRINT(stderr, "Failed to allocate context\n");
        exit(EXIT_FAILURE);
    }

    //this application will (so far) only handle one request at a time, so
    //allocate memory already here
    //todo: if we ever want to scale ...
    ctx->req = calloc(sizeof(struct tas_client_req), 1);

    if (!ctx->req) {
        TA_PRINT(stderr, "Failed to allocate client request\n");
        exit(EXIT_FAILURE);
    }

    configure_rt_tables(ctx, 4096);

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

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Started Table Allocator Server\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);

    //clean up allocated memory

    exit(EXIT_SUCCESS);
}
