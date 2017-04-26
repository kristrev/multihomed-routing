#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "table_allocator_server.h"
#include "table_allocator_server_sockets.h"

#include "table_allocator_log.h"
#include "table_allocator_libuv_helpers.h"

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

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Ready to start allocator\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);

    //clean up allocated memory

    exit(EXIT_SUCCESS);
}
