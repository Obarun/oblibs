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

#include <errno.h>
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

size_t istream_getnofill(istream *o, char *s, size_t len)
{
    return rbuffer_read(&o->rb, s, len) ;
}

ssize_t istream_fill(istream *o)
{
    struct iovec v[2] ;
    unsigned int n = rbuffer_wiovec(&o->rb, v) ;
    if (!n)
        return (errno = ENOBUFS, -1) ;   // buffer already full: nothing can be read in
    ssize_t r = io_readv(o->fd, v, (int)n) ;   // EINTR retried internally
    if (r > 0)
        rbuffer_wadvance(&o->rb, (size_t)r) ;
    return r ;
}

ssize_t istream_get(istream *o, char *s, size_t len)
{
    size_t w = 0 ;

    while (w < len) {
        w += istream_getnofill(o, s + w, len - w) ;   // drain buffered bytes
        if (w >= len)
            break ;
        ssize_t r = istream_fill(o) ;                 // buffer empty: one read
        if (r > 0)
            continue ;
        if (!r)
            break ;       // EOF -> return the short count gathered so far
        return -1 ;       // read error, or would-block on a non-blocking fd (errno set)
    }
    return (ssize_t)w ;
}
