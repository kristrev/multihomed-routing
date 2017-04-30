#ifndef TABLE_ALLOCATOR_CLIENT_NETLINK_H
#define TABLE_ALLOCATOR_CLIENT_NETLINK_H

struct nlattr;
struct tac_ctx;

struct nlattr_storage {
    const struct nlattr **tb;
    uint32_t limit;
};

//configure netlink + start listening
uint8_t table_allocator_client_netlink_configure(struct tac_ctx *ctx);

//stop netlink handling
void table_allocator_client_netlink_stop(struct tac_ctx *ctx);

#endif
