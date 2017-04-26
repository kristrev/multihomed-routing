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
