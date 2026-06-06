/*
 * socketunix_bind.c
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h> // socket in Unix
#include <sys/socket.h>

#include <oblibs/socket.h>

int socketunix_bind(int fd, const char *path)
{
    if (!path)
        return (errno = EINVAL, -1) ;
    struct sockaddr_un addr ;
    size_t len = strlen(path) ;
    if (len > SOCKETUNIX_PATH_MAX)
        return (errno = EPROTO, -1) ;
    memset(&addr, 0, sizeof(addr)) ;
    addr.sun_family = AF_UNIX ;
    memcpy(addr.sun_path, path, len + 1) ;

    return bind(fd, (struct sockaddr *) &addr, sizeof(addr)) ;
}
