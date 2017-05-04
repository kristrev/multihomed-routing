/*
 * Copyright 2017 Kristian Evensen <kristian.evensen@gmail.com>
 *
 * This file is part of Usb Monitor. Usb Monitor is free software: you can
 * redistribute it and/or modify it under the terms of the Lesser GNU General
 * Public License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * Usb Montior is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Usb Monitor. If not, see http://www.gnu.org/licenses/.
 */

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#include "table_allocator_shared_socket_helpers.h"

int ta_socket_helpers_create_unix_socket(const char *path_name)
{
    int32_t sock_fd;
	struct sockaddr_un local_addr;

    //subtracting two from size is because first byte is set to 0 since we used
    //abstract naming space, and to ensure that sun_path will be zero terminated
    if (path_name && strlen(path_name) > (sizeof(local_addr.sun_path) - 2)) {
        return -1;
    }

    if ((sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }

    if (path_name == NULL) {
        return sock_fd;
    }

	memset(&local_addr, 0, sizeof(local_addr));

    local_addr.sun_family = AF_UNIX;

    //we use abstract naming, so first byte of path is always \0
    strncpy(local_addr.sun_path + 1, path_name, strlen(path_name));

    if (bind(sock_fd, (struct sockaddr*) &local_addr,
				sizeof(local_addr)) == -1) {
		close(sock_fd);
		return -1;
	}

    return sock_fd;
}
