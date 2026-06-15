/*
 * socketunix_bind_reuse.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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

#include <oblibs/socket.h>
#include <oblibs/string.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>

int socketunix_bind_reuse(int fd, char const *path, int *fdlock)
{
    if (!path || !fdlock)
        return (errno = EINVAL, -1) ;

    size_t len = strlen(path) ;
    if (len > SOCKETUNIX_PATH_MAX)
        return (errno = EPROTO, -1) ;

    char lockname[SOCKETUNIX_PATH_MAX + 6] ; // "<path>.lock" + NUL, path <= 107
    auto_strings(lockname, path, ".lock") ;

    int lock = io_open_mode(lockname, O_WRONLY|O_NONBLOCK|O_CREAT|O_CLOEXEC, 0600) ;
    if (lock < 0)
        return -1 ;

    int r = lock_fd(lock, 1, 1) ;            // exclusive, non-blocking
    if (r < 0)
        return (close_fd(lock), -1) ;        // close_fd preserves errno
    if (!r)
        return (close_fd(lock), errno = EBUSY, -1) ;

    file_tryunlink(path) ;                   // drop any stale socket node

    if (socketunix_bind(fd, path) < 0)
        return (close_fd(lock), -1) ;

    *fdlock = lock ;
    return 0 ;
}
