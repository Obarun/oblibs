/*
 * istream.c
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

#include <oblibs/stream.h>

#include <stddef.h>
#include <stdint.h>
#include <sys/uio.h>

#include <oblibs/io.h>
#include <oblibs/rbuffer.h>

static uint8_t istream_0_storage[ISTREAM_INSIZE] ;
istream istream_0_ = { { istream_0_storage, ISTREAM_INSIZE, 0, 0 }, 0 } ;

int istream_init(istream *o, int fd, void *storage, size_t size)
{
    if (!rbuffer_init(&o->rb, storage, size))
        return 0 ;
    o->fd = fd ;
    return 1 ;
}

/* Pull one chunk from the fd into the ring buffer. Returns the raw io_readv
 * result: >0 bytes buffered, 0 on EOF, -1 on error. Called only when the buffer
 * has free room, so wiovec never reports full here. */
static ssize_t istream_fill(istream *o)
{
    struct iovec v[2] ;
    unsigned int n = rbuffer_wiovec(&o->rb, v) ;
    ssize_t r = io_readv(o->fd, v, (int)n) ;
    if (r > 0)
        rbuffer_wadvance(&o->rb, (size_t)r) ;
    return r ;
}

ssize_t istream_get(istream *o, char *s, size_t len)
{
    size_t w = 0 ;

    while (w < len) {
        w += rbuffer_read(&o->rb, s + w, len - w) ;   // drain buffered bytes
        if (w >= len)
            break ;
        ssize_t r = istream_fill(o) ;                 // buffer empty: refill
        if (r < 0)
            return -1 ;
        if (!r)
            break ;                                   // EOF
    }
    return (ssize_t)w ;
}
