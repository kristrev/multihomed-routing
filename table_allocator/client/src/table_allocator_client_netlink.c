#include <uv.h>
#include <string.h>
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/if_addr.h>
#include <linux/fib_rules.h>

#include <table_allocator_shared_log.h>

#include "table_allocator_client_netlink.h"
#include "table_allocator_client.h"

static int32_t table_allocator_client_netlink_modify_rule(struct tac_ctx *ctx,
        uint32_t msg_type, uint32_t flags, uint8_t prefix_len, uint8_t dir,
        uint32_t prio, const char *ifname)
{
    uint8_t buf[MNL_SOCKET_BUFFER_SIZE];
    struct nlmsghdr *nlh;
    struct rtmsg *rt;
    union {
        struct sockaddr_in *addr4;
        struct sockaddr_in6 *addr6;
    } u_addr;

    memset(buf, 0, MNL_SOCKET_BUFFER_SIZE);

    nlh = mnl_nlmsg_put_header(buf);
    nlh->nlmsg_type = msg_type;
    nlh->nlmsg_flags = NLM_F_REQUEST | flags;
    nlh->nlmsg_seq = 0;

    rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
    rt->rtm_family = ctx->address->addr_family;
    rt->rtm_dst_len = 0;
    rt->rtm_table = ctx->address->rt_table;
    rt->rtm_protocol = RTPROT_BOOT;
    rt->rtm_scope = RT_SCOPE_UNIVERSE;
    rt->rtm_type = RTN_UNICAST;
    
    mnl_attr_put_u32(nlh, FRA_PRIORITY, prio);
    mnl_attr_put_u32(nlh, FRA_TABLE, ctx->address->rt_table);

    if (dir == FRA_SRC) {
        rt->rtm_src_len = prefix_len;
    } else if (dir == FRA_DST) {
        rt->rtm_dst_len = prefix_len;
    }

    if (rt->rtm_src_len || rt->rtm_dst_len) {
        if (ctx->address->addr_family == AF_INET) {
            u_addr.addr4 = (struct sockaddr_in*) &(ctx->address->addr);
            mnl_attr_put_u32(nlh, dir, u_addr.addr4->sin_addr.s_addr);
        } else {
            u_addr.addr6 = (struct sockaddr_in6*) &(ctx->address->addr);
            mnl_attr_put(nlh, dir, sizeof(u_addr.addr6->sin6_addr.s6_addr),
                    u_addr.addr6->sin6_addr.s6_addr);
        }
    }

    if (ifname) {
        mnl_attr_put_strz(nlh, FRA_IFNAME, ifname);
    }

    return mnl_socket_sendto(ctx->rt_mnl_socket, nlh, nlh->nlmsg_len);
}

static void table_allocator_client_netlink_timeout_cb(uv_timer_t *handle)
{
    struct tac_ctx *ctx = handle->data;

    //we might have partial failures, causing update_rules to be called multiple
    //times when adding rules. This is not critical, if a rule is equal to an
    //existent rule then it will not be added
    table_allocator_client_netlink_update_rules(ctx, RTM_NEWRULE);
}

void table_allocator_client_netlink_update_rules(struct tac_ctx *ctx,
        uint32_t msg_type)
{
    int32_t retval = 0;

    retval = table_allocator_client_netlink_modify_rule(ctx, msg_type,
            NLM_F_CREATE | NLM_F_EXCL, 32, FRA_SRC, ADDR_RULE_PRIO, NULL);

    if (retval < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_INFO, "Failed to update rules. Ifname: %s "
                "address %s family %u table %u type %u\n",
                ctx->address->ifname, ctx->address->address_str,
                ctx->address->addr_family, ctx->address->rt_table, msg_type);

        //only start timeout on newrule, deleting rule when interface goes down
        //is a best-effort thing
        if (msg_type == RTM_NEWRULE) {
            uv_timer_start(&(ctx->netlink_timeout_handle),
                    table_allocator_client_netlink_timeout_cb,
                    TAC_NETLINK_TIMEOUT_MS, 0);
        }
        return;
    }

    retval = table_allocator_client_netlink_modify_rule(ctx, msg_type,
            NLM_F_CREATE | NLM_F_EXCL, ctx->address->subnet_prefix_len,
            FRA_DST, NW_RULE_PRIO, NULL);

    if (retval < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_INFO, "Failed to update rules. Ifname: %s "
                "address %s family %u table %u type %u\n",
                ctx->address->ifname, ctx->address->address_str,
                ctx->address->addr_family, ctx->address->rt_table, msg_type);

        if (msg_type == RTM_NEWRULE) {
            uv_timer_start(&(ctx->netlink_timeout_handle),
                    table_allocator_client_netlink_timeout_cb,
                    TAC_NETLINK_TIMEOUT_MS, 0);
        }
        return;
    }

    retval = table_allocator_client_netlink_modify_rule(ctx, msg_type,
            NLM_F_CREATE | NLM_F_EXCL, 0, FRA_DST, DEF_RULE_PRIO, "lo");

    if (retval < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_INFO, "Failed to update rules. Ifname: %s "
                "address %s family %u table %u type %u\n",
                ctx->address->ifname, ctx->address->address_str,
                ctx->address->addr_family, ctx->address->rt_table, msg_type);

        if (msg_type == RTM_NEWRULE) {
            uv_timer_start(&(ctx->netlink_timeout_handle),
                    table_allocator_client_netlink_timeout_cb,
                    TAC_NETLINK_TIMEOUT_MS, 0);
        }
    } else {
        TA_PRINT_SYSLOG(ctx, LOG_INFO, "Updated rules for ifname: %s "
                "address %s family %u table %u type %u\n",
                ctx->address->ifname, ctx->address->address_str,
                ctx->address->addr_family, ctx->address->rt_table, msg_type);

        ctx->address->rules_added = 1;
    }
};

static int table_allocator_client_netlink_parse_nlattr(
        const struct nlattr *attr, void *data)
{
    struct nlattr_storage *storage = (struct nlattr_storage*) data;
    int32_t type = mnl_attr_get_type(attr);

    if (mnl_attr_type_valid(attr, storage->limit) < 0)
        return MNL_CB_OK;

    storage->tb[type] = attr;
    return MNL_CB_OK;
}

static uint8_t table_allocator_client_netlink_handle_dellink(struct tac_ctx *ctx,
        struct nlmsghdr *nl_hdr)
{
    struct tac_address *address = ctx->address;
    struct ifinfomsg *ifi_msg = (struct ifinfomsg*)
        mnl_nlmsg_get_payload(nl_hdr);

    return ifi_msg->ifi_index == address->ifidx;
}

static uint8_t table_allocator_client_netlink_cmp_ip6addr(struct in6_addr *a,
        struct in6_addr *b)
{
    return a->s6_addr32[0] == b->s6_addr32[0] &&
        a->s6_addr32[1] == b->s6_addr32[1] &&
        a->s6_addr32[2] == b->s6_addr32[2] &&
        a->s6_addr32[3] == b->s6_addr32[3];
}

static uint8_t table_allocator_client_netlink_handle_deladdr(
        struct tac_ctx *ctx, struct nlmsghdr *nl_hdr)
{
    struct tac_address *address = ctx->address;
    struct ifaddrmsg *ifa_msg = (struct ifaddrmsg*)
        mnl_nlmsg_get_payload(nl_hdr);
    const struct nlattr *tb[IFA_MAX + 1] = {};
    struct nlattr_storage tb_storage = {tb, IFA_MAX};
    uint32_t *addr6_raw;
    uint8_t i;
    union {
        struct in_addr addr4;
        struct in6_addr addr6;
    } u_addr;

    union {
        struct sockaddr_in *sockaddr4;
        struct sockaddr_in6 *sockaddr6;
    } u_sockaddr;


    if (mnl_attr_parse(nl_hdr, sizeof(struct ifaddrmsg),
                table_allocator_client_netlink_parse_nlattr, &tb_storage) !=
                MNL_CB_OK) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Failed to parse netlink attrs\n");
        return 0;
    }

    if (ifa_msg->ifa_index != address->ifidx ||
        ifa_msg->ifa_family != address->addr_family ||
        ifa_msg->ifa_prefixlen != address->subnet_prefix_len) {
        return 0;
    }

    if (ifa_msg->ifa_family == AF_INET && tb[IFA_LOCAL]) {
        u_sockaddr.sockaddr4 = (struct sockaddr_in*) &(address->addr);
        u_addr.addr4.s_addr = mnl_attr_get_u32(tb[IFA_LOCAL]); 

        if (u_addr.addr4.s_addr != u_sockaddr.sockaddr4->sin_addr.s_addr) {
            return 0;
        }
    } else if (ifa_msg->ifa_family == AF_INET6 && tb[IFA_ADDRESS]) {
        u_sockaddr.sockaddr6 = (struct sockaddr_in6*) &(address->addr);
        addr6_raw = (uint32_t*) mnl_attr_get_payload(tb[IFA_ADDRESS]);

        //todo: remove this copy when testing ipv6 properly
        for (i = 0; i < 4; i++) {
            u_addr.addr6.s6_addr32[i] = *(addr6_raw + i);
        }

        if (!table_allocator_client_netlink_cmp_ip6addr(
                    &(u_sockaddr.sockaddr6->sin6_addr), &(u_addr.addr6))) {
            return 0;
        }
    }

    return 1;
}

static void table_allocator_client_netlink_recv_cb(uv_udp_t* handle,
        ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr,
        unsigned flags)
{
    struct tac_ctx *ctx = handle->data;
    struct tac_address *address = ctx->address;
    struct nlmsghdr *nl_hdr = (struct nlmsghdr*) buf->base;
    //this cast is to prevent getting compilation warnings on mnl_nlmsg_next
    int32_t numbytes = (int32_t) nread;

    if (nread == 0) {
        return;
    } else if (nread < 0) {
        TA_PRINT_SYSLOG(ctx, LOG_ERR, "Netlink socket read error\n");
        return;
    }

    while (mnl_nlmsg_ok(nl_hdr, numbytes)) {
        if (nl_hdr->nlmsg_type == RTM_DELLINK) {
            //todo: if this returns true, just stop
            if (table_allocator_client_netlink_handle_dellink(ctx, nl_hdr)) {
                TA_PRINT_SYSLOG(ctx, LOG_DEBUG,
                        "Will flush rules and stop after DELLINK "
                        "(fam. %u if. %s addr, %s/%u)\n", address->addr_family,
                        address->ifname, address->address_str,
                        address->subnet_prefix_len);

                //flush rules
                table_allocator_client_netlink_update_rules(ctx, RTM_DELRULE);
                uv_stop(&(ctx->event_loop));
            }
        } else if (nl_hdr->nlmsg_type == RTM_DELADDR) {
            if (table_allocator_client_netlink_handle_deladdr(ctx, nl_hdr)) {
                TA_PRINT_SYSLOG(ctx, LOG_DEBUG,
                        "Will flush rules and stop after DELADDR "
                        "(fam. %u if. %s addr, %s/%u)\n", address->addr_family,
                        address->ifname, address->address_str,
                        address->subnet_prefix_len);

                //flush rules
                table_allocator_client_netlink_update_rules(ctx, RTM_DELRULE);
                uv_stop(&(ctx->event_loop));
            }
        }

        nl_hdr = mnl_nlmsg_next(nl_hdr, &numbytes);
    }
}

static void table_allocator_client_netlink_alloc_cb(uv_handle_t *handle_size,
        size_t suggested_size, uv_buf_t *buf)
{
    struct tac_ctx *ctx = handle_size->data;

    memset(ctx->mnl_recv_buf, 0, MNL_SOCKET_BUFFER_SIZE);
    buf->base = (char*) ctx->mnl_recv_buf;
    buf->len = MNL_SOCKET_BUFFER_SIZE;
}

uint8_t table_allocator_client_netlink_configure(struct tac_ctx *ctx)
{
    struct mnl_socket *mnl_sock = NULL;

    if ((mnl_sock = mnl_socket_open(NETLINK_ROUTE)) == NULL) {
        TA_PRINT(stderr, "Failed to open netlink socket\n");
        return 0;
    }

    //todo: make this depend on which address family is set
    if (mnl_socket_bind(mnl_sock, (1 << (RTNLGRP_IPV4_IFADDR - 1)) |
                (1 << (RTNLGRP_LINK - 1)), MNL_SOCKET_AUTOPID) < 0)
    {
        TA_PRINT(stderr,  "Failed to bind netlink socket\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    ctx->rt_mnl_socket = mnl_sock;

    if (uv_timer_init(&(ctx->event_loop), &(ctx->netlink_timeout_handle))) {
        TA_PRINT(stderr, "Initializing netlink timer failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    ctx->netlink_timeout_handle.data = ctx;

    if (uv_udp_init(&(ctx->event_loop), &(ctx->netlink_handle))) {
        TA_PRINT(stderr, "Intializing netlink udp handle failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    if (uv_udp_open(&(ctx->netlink_handle), mnl_socket_get_fd(mnl_sock))) {
        TA_PRINT(stderr, "Opening netlink udp handle failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    if (uv_udp_recv_start(&(ctx->netlink_handle),
                table_allocator_client_netlink_alloc_cb,
                table_allocator_client_netlink_recv_cb)) {
        TA_PRINT(stderr, "Starting netlink receive failed\n");
        mnl_socket_close(mnl_sock);
        return 0;
    }

    ctx->netlink_handle.data = ctx;

    return 1;
}

void table_allocator_client_netlink_stop(struct tac_ctx *ctx)
{
    //todo: we should really do uv_clos here to make sure we shut down cleanly,
    //but as long as this function is only used right before application exits,
    //we don't really have to
    uv_udp_recv_stop(&(ctx->netlink_handle));
    mnl_socket_close(ctx->rt_mnl_socket);
}
