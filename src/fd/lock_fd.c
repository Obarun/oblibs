/*
 * lock_fd.c
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

#include <sys/file.h>
#include <errno.h>

#include <oblibs/fd.h>

int lock_fd(int fd, int ex, int nb)
{
    int e = errno ;
    int op = (ex ? LOCK_EX : LOCK_SH) | (nb ? LOCK_NB : 0) ;

    while (flock(fd, op) < 0) {
        if (errno == EINTR)
            continue ;
        if (nb && (errno == EWOULDBLOCK))
            return 0 ;
        return -1 ;
    }

    errno = e ;
    return 1 ;
}
