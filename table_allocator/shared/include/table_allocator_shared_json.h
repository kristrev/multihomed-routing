#ifndef TABLE_ALLOCATOR_CLIENT_JSON_H
#define TABLE_ALLOCATOR_CLIENT_JSON_H

#include <json-c/json.h>

#define TA_SHARED_JSON_ADDRESS_KEY  "address"
#define TA_SHARED_JSON_FAMILY_KEY   "addr_family"
#define TA_SHARED_JSON_IFNAME_KEY   "ifname"
#define TA_SHARED_JSON_TAG_KEY      "tag"
#define TA_SHARED_JSON_CMD_KEY      "cmd"

//request or release
#define TA_SHARE_CMD_REQ            0
#define TA_SHARE_CMD_REL            1

//maximum size of json object we currently can export
#define TA_SHARE_MAX_JSON_LEN       242

struct json_object *table_allocator_shared_json_create_req(const char *address,
        const char *ifname, const char *tag, uint8_t addr_family, uint8_t cmd);

#endif
