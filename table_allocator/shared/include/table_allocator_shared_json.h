#ifndef TABLE_ALLOCATOR_CLIENT_JSON_H
#define TABLE_ALLOCATOR_CLIENT_JSON_H

#include <json-c/json.h>

#define TA_SHARED_JSON_ADDRESS_KEY  "address"
#define TA_SHARED_JSON_FAMILY_KEY   "addr_family"
#define TA_SHARED_JSON_IFNAME_KEY   "ifname"
#define TA_SHARED_JSON_TAG_KEY      "tag"
#define TA_SHARED_JSON_CMD_KEY      "cmd"
#define TA_SHARED_JSON_VERSION_KEY  "version"

//todo: move to new header file table_allocator_shared.h
//request or release are the only two commands we support now
#define TA_SHARE_CMD_REQ            0
#define TA_SHARE_CMD_REL            1

//todo: should perhaps move this somewhere else?
#define TA_VERSION                  1

//maximum size of json object we currently can export (+ \0)
#define TA_SHARED_MAX_JSON_LEN      254

#define TA_SHARED_MAX_TAG_SIZE      128
//max. len. for unix domain sockets is 108, but string needs to be zero
//teminated and we loose one byte (in front) since we use abstract naming
//this is DESTINATION address, limit for address to allocate is INET6_ADDRSTRLEN
#define TA_SHARED_MAX_ADDR_SIZE     106

struct json_object *table_allocator_shared_json_create_req(const char *address,
        const char *ifname, const char *tag, uint8_t addr_family, uint8_t cmd);

uint8_t table_allocator_shared_json_parse_seq(const char *json_obj_char,
        uint8_t *addr_family, uint8_t *cmd, uint8_t *ver, char *addr_str,
        char *ifname_str, char *tag_str);
#endif
