/*
 * io_readnclose.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/types.h>
#include <stddef.h>
#include <errno.h>

#include <oblibs/io.h>
#include <oblibs/fd.h>

ssize_t io_readnclose(int fd, char *s, size_t n)
{
    if (fd < 0)
        return -1 ;

    size_t r = io_allread(fd, s, n) ;
    close_fd(fd) ; /* close_fd preserves errno (set by io_allread on short read) */

    if (r < n && errno != EPIPE)   /* short read not caused by EOF = real error */
        return -1 ;

    return (ssize_t)r ;
}
