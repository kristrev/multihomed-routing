#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <json-c/json.h>
#include <string.h>

#include "table_allocator_server.h"
#include "table_allocator_server_sockets.h"

#include <table_allocator_shared_log.h>
#include <table_allocator_shared_libuv_helpers.h>

static void populate_table_map(uint32_t *table_map, uint32_t num_elems)
{
    uint32_t i;

    for (i = 0; i < num_elems; i++) {
        table_map[i] = UINT32_MAX;
    }
}

static uint8_t configure_rt_tables(struct tas_ctx *ctx, uint8_t addr_families)
{
    uint32_t num_table_elements = 0;

    //comput the number of entries we need in the table, done by diving the
    //maximum number of elements on 32
    if (ctx->num_tables < 32) {
        num_table_elements = 0;
    } else if (ctx->num_tables & 0x1F) {
        //If number of values is not divisible by 32, I need an additional
        //element to store the remainders. Divisibility is checked by masking
        //with 31. If any bit is set, then value is not divisible by 32
        num_table_elements = (ctx->num_tables >> 5) + 1;
    } else {
        num_table_elements = ctx->num_tables >> 5;
    }

    if (addr_families & ADDR_FAMILY_INET) {
        if (!(ctx->tables_inet = calloc(sizeof(uint32_t) * num_table_elements,
                        1))) {
        TA_PRINT(stderr, "Failed to allocate v4-tables\n");
        return 0;
        } else {
            populate_table_map(ctx->tables_inet, num_table_elements);
        }
    }

    if (addr_families & ADDR_FAMILY_INET6) {
        if (!(ctx->tables_inet6 = calloc(sizeof(uint32_t) * num_table_elements,
                        1))) {
            TA_PRINT(stderr, "Failed to allocate v6-tables\n");
            return 0;
        } else {
            populate_table_map(ctx->tables_inet6, num_table_elements);
        }
    }

    if (addr_families & ADDR_FAMILY_UNSPEC) {
        if (!(ctx->tables_unspec = calloc(sizeof(uint32_t) * num_table_elements,
                        1))) {
            TA_PRINT(stderr, "Failed to allocate unspec-tables\n");
            return 0;
        } else {
            populate_table_map(ctx->tables_unspec, num_table_elements);
        }
    }

    ctx->num_table_elements = num_table_elements;

    return 1;
}

static void parse_addr_families(struct json_object *fam_obj, uint8_t *mask)
{
    json_bool add_addr_family;

    json_object_object_foreach(fam_obj, key, val) {
        if (!strcmp(key, TAS_ADDR_FAMILIES_INET_KEY) &&
                json_object_is_type(val, json_type_boolean)) {
            add_addr_family = json_object_get_boolean(val);
            if (add_addr_family)
                *mask |= ADDR_FAMILY_INET;
        } else if (!strcmp(key, TAS_ADDR_FAMILIES_INET6_KEY) &&
                json_object_is_type(val, json_type_boolean)) {
            add_addr_family = json_object_get_boolean(val);
            if (add_addr_family)
                *mask |= ADDR_FAMILY_INET6;
        } else if (!strcmp(key, TAS_ADDR_FAMILIES_UNSPEC_KEY) &&
                json_object_is_type(val, json_type_boolean)) {
            add_addr_family = json_object_get_boolean(val);
            if (add_addr_family)
                *mask |= ADDR_FAMILY_UNSPEC;
        }
    }
}

static uint8_t parse_config(struct tas_ctx *ctx, const char *conf_file_path)
{
    uint8_t conf_file_buf[1024] = {0};
    uint8_t addr_fam_mask = 0;
    FILE *conf_file = NULL;
    size_t num_bytes;
    struct json_object *conf_obj, *addr_families_obj = NULL;
    const char *socket_path = NULL, *db_path = NULL, *log_path = NULL;

    if (!(conf_file = fopen(conf_file_path, "r"))) {
        TA_PRINT(stderr, "Failed to open config file: %s\n", conf_file_path);
        return 0;
    }

    num_bytes = fread(conf_file_buf, sizeof(conf_file_buf), 1, conf_file);

    if (ferror(conf_file)) {
        TA_PRINT(stderr, "Reading config file failed\n");
        fclose(conf_file);
        return 0;
    }

    fclose(conf_file);

    //todo: add better handling here!
    if (num_bytes == sizeof(conf_file_buf)) {
        TA_PRINT(stderr, "Buffer too small to store config file\n");
        return 0;
    }

    if (!(conf_obj = json_tokener_parse((const char*) conf_file_buf))) {
        TA_PRINT(stderr, "Could not parse config json\n");
        return 0;
    }

    json_object_object_foreach(conf_obj, key, val) {
        if (!strcmp(key, TAS_SOCKET_PATH_KEY) &&
                json_object_is_type(val, json_type_string)) {
            //path to domain socket
            socket_path = json_object_get_string(val);
        } else if (!strcmp(key, TAS_TABLE_OFFSET_KEY) &&
                json_object_is_type(val, json_type_int)) {
            //table offset (i.e., first table returned will be this value)
            ctx->table_offset = json_object_get_int(val);
        } else if (!strcmp(key, TAS_NUM_TABLES_KEY) &&
                json_object_is_type(val, json_type_int)) {
            //how many tables we can allocate
            ctx->num_tables = json_object_get_int(val);
        } else if (!strcmp(key, TAS_TABLE_TIMEOUT_KEY) &&
                json_object_is_type(val, json_type_int)) {
            //how long a table lease is valid for (a lease has to updated within
            //this interval)
            ctx->table_timeout = json_object_get_int(val);
        } else if (!strcmp(key, TAS_DB_PATH_KEY) &&
                json_object_is_type(val, json_type_string)) {
            //path to the db used to store leases (we use sqlite3 for now)
            db_path = json_object_get_string(val);
        } else if (!strcmp(key, TAS_DO_SYSLOG_KEY) &&
                json_object_is_type(val, json_type_boolean)) {
            //if we should write to syslog (default to false)
            ctx->use_syslog = json_object_get_boolean(val);
        } else if (!strcmp(key, TAS_LOG_PATH_KEY) &&
                json_object_is_type(val, json_type_string)) {
            //log path (default stderr)
            log_path = json_object_get_string(val);
        } else if (!strcmp(key, TAS_ADDR_FAMILIES_KEY) &&
                json_object_is_type(val, json_type_object)) {
            //address families object, will be parsed later
            addr_families_obj = val; 
        }
    }

    if (!socket_path || !ctx->table_offset || !ctx->num_tables ||
            !ctx->table_timeout || !db_path || !addr_families_obj) {
        TA_PRINT(stderr, "Required argument is missing\n");
        json_object_put(conf_obj);
        return 0;
    }

    if (strlen(socket_path) > TA_SHARED_MAX_ADDR_SIZE) {
        TA_PRINT(stderr, "Socket path is too long\n");
        json_object_put(conf_obj);
        return 0;
    } else {
        memcpy(ctx->socket_path, socket_path, strlen(socket_path));
    }

    if (strlen(db_path) >= MAX_DB_PATH_LEN) {
        TA_PRINT(stderr, "Database path is too long\n");
        json_object_put(conf_obj);
        return 0;
    } else {
        memcpy(ctx->db_path, db_path, strlen(db_path));
    }

    if (((uint32_t) (ctx->table_offset + ctx->num_tables)) <=
            ctx->table_offset) {
        TA_PRINT(stderr, "Table counter is wrapping\n");
        json_object_put(conf_obj);
        return 0;
    }

    if (log_path && !(ctx->logfile = fopen(log_path, "a"))) {
        //remember that logfile might be NULL here
        TA_PRINT(stderr, "Could not open logilfe: %s\n", log_path);
        json_object_put(conf_obj);
        return 0;
    }

    //check families
    parse_addr_families(addr_families_obj, &addr_fam_mask);

    if (!addr_fam_mask) {
        TA_PRINT(stderr, "Could not open logilfe: %s\n", log_path);
        json_object_put(conf_obj);
    }

    if (!configure_rt_tables(ctx, addr_fam_mask)) {
        TA_PRINT(stderr, "Failed to configure routing tables\n");
        json_object_put(conf_obj);
        return 0;
    }

    json_object_put(conf_obj);

    return 1;
}

int main(int argc, char *argv[])
{
    struct tas_ctx *ctx;
    const char *conf_file_path = NULL;
    int32_t opt;

    //parse the one command line argument we support
    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
        case 'c':
            conf_file_path = optarg;
            break;
        default:
            TA_PRINT(stderr, "Got unknown argument (%c)\n", opt);
            exit(EXIT_FAILURE);
        }
    }

    if (!conf_file_path) {
        TA_PRINT(stderr, "Missing configuration file (specified with -c)\n");
        exit(EXIT_FAILURE);
    }

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

    ctx->logfile = stderr;
    ctx->use_syslog = 0;
    
    //create event loop
    if (uv_loop_init(&(ctx->event_loop))) {
        TA_PRINT(stderr, "Event loop creation failed\n");
        exit(EXIT_FAILURE);
    }

    //parse config
    if (!parse_config(ctx, conf_file_path)) {
        TA_PRINT(stderr, "Option parsing failed\n");    
        exit(EXIT_FAILURE);
    }
  
    if (!ta_allocator_libuv_helpers_configure_unix_handle(&(ctx->event_loop),
                &(ctx->unix_socket_handle), &(ctx->unix_socket_timeout_handle),
                unix_socket_timeout_cb, ctx)) {
        TA_PRINT(stderr, "Failed to configure domain handle\n");
        exit(EXIT_FAILURE);
    }

    TA_PRINT_SYSLOG(ctx, LOG_INFO, "Started Table Allocator Server\n"
           "\tSocket path: %s\n"
           "\tDatabase path: %s\n"
           "\tNum. tables: %u\n"
           "\tTable offset: %u\n"
           "\tMax. table number: %u\n"
           "\tTable timeout: %u sec\n", ctx->socket_path, ctx->db_path,
           ctx->num_tables, ctx->table_offset,
           ctx->num_tables + ctx->table_offset, ctx->table_timeout);

    uv_run(&(ctx->event_loop), UV_RUN_DEFAULT);

    //clean up allocated memory

    exit(EXIT_SUCCESS);
}
