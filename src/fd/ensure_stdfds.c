/*
 * ensure_stdfds.c
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

#include <fcntl.h>
#include <errno.h>

#include <oblibs/fd.h>
#include <oblibs/io.h>

/* If @fd is closed (F_GETFD fails with EBADF), open /dev/null and move it onto
 * @fd (write-only when @w, else read-only). Already-open @fd is left as is.
 * Internal: 66 only needs ensure_stdfds, not a public fd_ensure_open. */
static int ensure_open(int fd, int w)
{
    if (fcntl(fd, F_GETFD) == -1) {
        if (errno != EBADF)
            return 0 ;
        int newfd = io_open("/dev/null", w ? O_WRONLY : O_RDONLY) ;
        if (newfd == -1)
            return 0 ;
        if (move_fd(fd, newfd) == -1) {
            close_fd(newfd) ;
            return 0 ;
        }
    }
    return 1 ;
}

int ensure_stdfds(void)
{
    if (!ensure_open(2, 1)) return 0 ;
    if (!ensure_open(1, 1)) return 0 ;
    if (!ensure_open(0, 0)) return 0 ;
    return 1 ;
}
