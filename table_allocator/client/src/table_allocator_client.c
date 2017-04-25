#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "table_allocator_client.h"
#include "table_allocator_log.h"

int main(int argc, char *argv[])
{
    struct tac_ctx *ctx;

    //parse command line arguments

    //create the application context
    ctx = calloc(sizeof(struct tac_ctx), 1);

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
    /*if (!parse_config(ctx, NULL)) {
        TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Option parsing failed\n");    
        exit(EXIT_FAILURE);
    }*/
  
    /*if (!configure_domain_handle(ctx)) {
       TA_PRINT_SYSLOG(ctx, LOG_CRIT, "Failed to configure domain handle\n");
        exit(EXIT_FAILURE);
    }*/

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Ready to start allocator client\n");

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);

    //clean up allocated memory

    exit(EXIT_SUCCESS);

    exit(EXIT_SUCCESS);
}
