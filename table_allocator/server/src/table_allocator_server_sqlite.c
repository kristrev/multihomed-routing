#include <string.h>

#include <table_allocator_shared_log.h>

#include "table_allocator_server_sqlite.h"
#include "table_allocator_server.h"

uint8_t table_allocator_server_sqlite_create_db(struct tas_ctx *ctx)
{
    sqlite3 *db_handle = NULL;
    int retval = 0;
    char *db_errmsg = NULL;

    retval = sqlite3_open_v2((const char*) ctx->db_path, &db_handle,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            NULL);

    if (retval != SQLITE_OK) {
        if (db_handle != NULL)
            TA_PRINT(stderr, "open failed with message: %s\n",
                    sqlite3_errmsg(db_handle));
        else
            TA_PRINT(stderr, "not enough memory to create db_handle object\n");

        return 0;
    }

    if (sqlite3_exec(db_handle, CREATE_SQL, NULL, NULL, &db_errmsg)) {
        TA_PRINT(stderr, "db create failed with message: %s\n",
                db_errmsg);
        sqlite3_close_v2(db_handle);
        return 0;
    }

    if (sqlite3_prepare_v2(db_handle, SELECT_RT_TABLE, -1,
                &(ctx->select_rt_table), NULL) ||
       sqlite3_prepare_v2(db_handle, INSERT_RT_TABLE, -1,
                &(ctx->insert_rt_table), NULL) ||
       sqlite3_prepare_v2(db_handle, DELETE_RT_TABLE, -1,
                &(ctx->delete_rt_table), NULL) ||
       sqlite3_prepare_v2(db_handle, UPDATE_RT_TABLE, -1,
                &(ctx->update_rt_table), NULL) ||
       sqlite3_prepare_v2(db_handle, SELECT_DEAD_LEASES, -1,
                &(ctx->select_dead_leases), NULL) ||
       sqlite3_prepare_v2(db_handle, DELETE_DEAD_LEASES, -1,
                &(ctx->delete_dead_leases), NULL)) {
        TA_PRINT(stderr, "Statement failed: %s\n", sqlite3_errmsg(db_handle));
        sqlite3_close_v2(db_handle);
        return 0;
    }

    ctx->db_handle = db_handle;
    return 1;
}

uint8_t table_allocator_sqlite_insert_table(struct tas_ctx *ctx,
        struct tas_client_req *req, uint32_t rt_table, time_t lease_sec)
{
    int32_t retval;

    sqlite3_clear_bindings(ctx->insert_rt_table);
    sqlite3_reset(ctx->insert_rt_table);

    if (sqlite3_bind_int(ctx->insert_rt_table, 1, rt_table) ||
        sqlite3_bind_int(ctx->insert_rt_table, 2, req->addr_family) ||
        sqlite3_bind_text(ctx->insert_rt_table, 3, req->ifname,
            strlen(req->ifname), SQLITE_STATIC) ||
        sqlite3_bind_text(ctx->insert_rt_table, 4, req->address,
            strlen(req->ifname), SQLITE_STATIC) ||
        sqlite3_bind_int(ctx->insert_rt_table, 5, lease_sec)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to bind INSERT values\n");
        return 0;
    }

    if (req->tag[0] != 0 && sqlite3_bind_text(ctx->insert_rt_table, 6,
                req->tag, strlen(req->tag), SQLITE_STATIC)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to bind INSERT values (tag)\n");
        return 0;
    }

    retval = sqlite3_step(ctx->insert_rt_table);

    if (retval != SQLITE_DONE) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Insert table failed: %s\n",
                sqlite3_errstr(retval));
        return 0;
    }

    return 1;
}

uint32_t table_allocator_sqlite_get_table(struct tas_ctx *ctx,
        struct tas_client_req *req)
{
    uint32_t rt_table = 0;

    sqlite3_clear_bindings(ctx->select_rt_table);
    sqlite3_reset(ctx->select_rt_table);

    if (sqlite3_bind_int(ctx->select_rt_table, 1, req->addr_family) ||
        sqlite3_bind_text(ctx->select_rt_table, 2, req->ifname,
            strlen(req->ifname), SQLITE_STATIC) ||
        sqlite3_bind_text(ctx->select_rt_table, 3, req->address,
            strlen(req->ifname), SQLITE_STATIC)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to bind SELECT values\n");
        return 0;
    }

    if (sqlite3_step(ctx->select_rt_table) == SQLITE_ROW) {
        rt_table = (uint32_t) sqlite3_column_int(ctx->select_rt_table, 0);
    }

    return rt_table;
}

uint8_t table_allocator_sqlite_remove_table(struct tas_ctx *ctx,
        struct tas_client_req *req)
{
    int32_t retval;

    sqlite3_clear_bindings(ctx->delete_rt_table);
    sqlite3_reset(ctx->delete_rt_table);

    if (sqlite3_bind_int(ctx->delete_rt_table, 1, req->addr_family) ||
        sqlite3_bind_text(ctx->delete_rt_table, 2, req->ifname,
            strlen(req->ifname), SQLITE_STATIC) ||
        sqlite3_bind_text(ctx->delete_rt_table, 3, req->address,
            strlen(req->ifname), SQLITE_STATIC)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to bind SELECT values\n");
        return 0;
    }

    retval = sqlite3_step(ctx->delete_rt_table);

    if (retval != SQLITE_DONE) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Delete table failed: %s\n",
                sqlite3_errstr(retval));
        return 0;
    }

    return 1;
}

uint8_t table_allocator_sqlite_update_lease(struct tas_ctx *ctx,
        uint32_t rt_table, uint8_t addr_family, uint32_t lease_sec)
{
    int32_t retval;

    sqlite3_clear_bindings(ctx->update_rt_table);
    sqlite3_reset(ctx->update_rt_table);

    if (sqlite3_bind_int(ctx->update_rt_table, 1, lease_sec) ||
        sqlite3_bind_int(ctx->update_rt_table, 2, addr_family) ||
        sqlite3_bind_int(ctx->update_rt_table, 3, rt_table)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to bind SELECT values\n");
        return 0;
    }

    retval = sqlite3_step(ctx->update_rt_table);

    if (retval != SQLITE_DONE) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Update table failed: %s\n",
                sqlite3_errstr(retval));
        return 0;
    }

    return 1;
}

uint8_t table_allocator_sqlite_delete_dead_leases(struct tas_ctx *ctx,
        uint32_t lease_limit, check_table_cb cb)
{
    int32_t retval;
    uint32_t rt_table;
    uint8_t addr_family;
    sqlite3_clear_bindings(ctx->select_dead_leases);
    sqlite3_reset(ctx->select_dead_leases);
    sqlite3_clear_bindings(ctx->delete_dead_leases);
    sqlite3_reset(ctx->delete_dead_leases);

    if (sqlite3_bind_int(ctx->select_dead_leases, 1, lease_limit)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Bind SELECT dead leases failed\n");
        return 0;
    }

    while (sqlite3_step(ctx->select_dead_leases) == SQLITE_ROW) {
        rt_table = (uint32_t) sqlite3_column_int(ctx->select_dead_leases, 0);
        addr_family = (uint8_t) sqlite3_column_int(ctx->select_dead_leases, 1);
        cb(ctx, addr_family, rt_table);
    }

    if (sqlite3_bind_int(ctx->delete_dead_leases, 1, lease_limit)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Bind DELETE dead leases failed\n");
        return 0;
    }

    retval = sqlite3_step(ctx->delete_dead_leases);

    //this is not a critical error, we guard against dead leases when
    //reconstructing map from database and we have ON CONFLICT REPLACE on the
    //table. So deleting dead leases is done to be nice
    if (retval != SQLITE_DONE) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Deleting dead leases failed\n");
    }

    return 1;
}

uint8_t table_allocator_sqlite_build_table_map(struct tas_ctx *ctx,
        uint32_t lease_limit, check_table_cb cb)
{
    uint32_t rt_table;
    uint8_t addr_family;
    int32_t retval;
    sqlite3_stmt *get_active_leases;

    if ((retval = sqlite3_prepare_v2(ctx->db_handle, SELECT_ALIVE_LEASES, -1,
                    &get_active_leases, NULL))) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Preparing SELECT_ALIVE_LEASES failed."
                " Error: %s\n", sqlite3_errstr(retval));
        return 0;
    }

    if (sqlite3_bind_int(get_active_leases, 1, lease_limit)) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Bind SELECT_ALIVE_LEASES failed\n");
        sqlite3_finalize(get_active_leases);
        return 0;
    }

    while (sqlite3_step(get_active_leases) == SQLITE_ROW) {
        rt_table = (uint32_t) sqlite3_column_int(get_active_leases, 0);
        addr_family = (uint8_t) sqlite3_column_int(get_active_leases, 1);
        cb(ctx, addr_family, rt_table);
    }

    sqlite3_finalize(get_active_leases);
    return 1;
}
