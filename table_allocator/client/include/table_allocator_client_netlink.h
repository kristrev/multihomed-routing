#ifndef TABLE_ALLOCATOR_CLIENT_NETLINK_H
#define TABLE_ALLOCATOR_CLIENT_NETLINK_H

#define ADDR_RULE_PRIO          10000
#define NW_RULE_PRIO            20000
#define DEF_RULE_PRIO           91000

//how long to wait until we try to add rules again
#define TAC_NETLINK_TIMEOUT_MS  1000

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

//add rules. If adding rules fails, then we will start a timer
void table_allocator_client_netlink_update_rules(struct tac_ctx *ctx,
        uint32_t msg_type);

#endif
