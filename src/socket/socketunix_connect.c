/*
 * socketunix_connect.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <oblibs/socket.h>

int socketunix_connect(int fd, const char *path)
{
    union {
		struct sockaddr_un u ; // unix
		struct sockaddr g ; // generic
	} addr = { 0 } ;

    if (!path)
        return (errno = EINVAL, -1) ;

    size_t len = strlen(path) ;
    if (len > SOCKETUNIX_PATH_MAX)
        return (errno = ENAMETOOLONG, -1) ;

    addr.u.sun_family = AF_UNIX ;
    memset(&addr.u.sun_path, 0, sizeof(addr.u.sun_path)) ;
    memcpy(addr.u.sun_path, path, len) ;
    addr.u.sun_path[len] = 0 ;
    socklen_t size = offsetof(struct sockaddr_un, sun_path) + len + 1 ;

    if (connect(fd, &addr.g, size) == -1)
        return -1 ;

    return 0 ;
}