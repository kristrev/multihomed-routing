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

