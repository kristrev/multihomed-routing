#ifndef TABLE_ALLOCATOR_SHARED_SOCKET_HELPERS_H
#define TABLE_ALLOCATOR_SHARED_SOCKET_HELPERS_H

#define DOMAIN_SOCKET_TIMEOUT_MS 200

int ta_socket_helpers_create_unix_socket(const char *path);

#endif
