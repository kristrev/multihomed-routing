#include <string.h>
#include <netinet/in.h>
#include <linux/if.h>

#include "table_allocator_shared_json.h"

struct json_object *table_allocator_shared_json_create_req(const char *address,
        const char *ifname, const char *tag, uint8_t addr_family, uint8_t cmd)
{
    struct json_object *json_obj = NULL, *obj_add;

    if (!(json_obj = json_object_new_object())) {
        return NULL;
    }

    if (!(obj_add = json_object_new_string(address))) {
        json_object_put(json_obj);
        return NULL;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_ADDRESS_KEY, obj_add);

    if (!(obj_add = json_object_new_string(ifname))) {
        json_object_put(json_obj);
        return NULL;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_IFNAME_KEY, obj_add);

    if (tag) {
        if (!(obj_add = json_object_new_string(tag))) {
            json_object_put(json_obj);
            return NULL;
        }
        json_object_object_add(json_obj, TA_SHARED_JSON_TAG_KEY, obj_add);
    }

    if (!(obj_add = json_object_new_int(addr_family))) {
        json_object_put(json_obj);
        return NULL;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_FAMILY_KEY, obj_add);

    if (!(obj_add = json_object_new_int(cmd))) {
        json_object_put(json_obj);
        return NULL;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_CMD_KEY, obj_add);

    if (!(obj_add = json_object_new_int(TA_VERSION))) {
        json_object_put(json_obj);
        return NULL;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_VERSION_KEY, obj_add);

    return json_obj;
}

uint8_t table_allocator_shared_json_parse_seq(const char *json_obj_char,
        uint8_t *addr_family, uint8_t *cmd, uint8_t *ver, char *addr_str,
        char *ifname_str, char *tag_str)
{
    struct json_object *json_obj;
    const char *str_val;

    if (!(json_obj = json_tokener_parse(json_obj_char))) {
        return 0;
    }

    json_object_object_foreach(json_obj, key, val) {
        if (!strcmp(key, TA_SHARED_JSON_ADDRESS_KEY) &&
                json_object_is_type(val, json_type_string)) {
            str_val = json_object_get_string(val);

            if (strlen(str_val) < INET6_ADDRSTRLEN) {
                memcpy(addr_str, str_val, strlen(str_val));
            }
        } else if (!strcmp(key, TA_SHARED_JSON_FAMILY_KEY) &&
                json_object_is_type(val, json_type_int)) {
            *addr_family = json_object_get_int(val);
        } else if (!strcmp(key, TA_SHARED_JSON_IFNAME_KEY) &&
                json_object_is_type(val, json_type_string)) {
            str_val = json_object_get_string(val);

            if (strlen(str_val) < IFNAMSIZ) {
                memcpy(ifname_str, str_val, strlen(str_val));
            }
        } else if (!strcmp(key, TA_SHARED_JSON_TAG_KEY) &&
                json_object_is_type(val, json_type_string)) {
            str_val = json_object_get_string(val);
     
            if (strlen(str_val) < TA_SHARED_MAX_TAG_SIZE) {
                memcpy(tag_str, str_val, strlen(str_val));
            }
        } else if (!strcmp(key, TA_SHARED_JSON_CMD_KEY) &&
                json_object_is_type(val, json_type_int)) {
            *cmd = json_object_get_int(val);
        } else if (!strcmp(key, TA_SHARED_JSON_VERSION_KEY) &&
                json_object_is_type(val, json_type_int)) {
            *ver = json_object_get_int(val);
        }
    }

    json_object_put(json_obj);
    return 1;
}

uint32_t table_allocator_shared_json_gen_response(uint32_t table, uint8_t *buf)
{
    struct json_object *json_obj = NULL, *obj_add;
    const char *json_str;
    uint32_t buf_len;

    if (!(json_obj = json_object_new_object())) {
        return 0;
    }

    if (!(obj_add = json_object_new_int(TA_VERSION))) {
        json_object_put(json_obj);
        return 0;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_VERSION_KEY, obj_add);

    if (!(obj_add = json_object_new_int(TA_SHARED_CMD_RESP))) {
        json_object_put(json_obj);
        return 0;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_CMD_KEY, obj_add);

    if (!(obj_add = json_object_new_int64(table))) {
        json_object_put(json_obj);
        return 0;
    }
    json_object_object_add(json_obj, TA_SHARED_JSON_TABLE_KEY, obj_add);

    json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN);

    //remember that buffer is expected to be 0 terminated (or at least that
    //there is room for the 0 byte on receive)
    if (strlen(json_str) >= TA_SHARED_MAX_JSON_LEN) {
        json_object_put(json_obj);
        return 0;
    }

    memcpy(buf, json_str, strlen(json_str));
    buf_len = strlen(json_str);
    json_object_put(json_obj);

    return buf_len;
}
