/*
 * ostream.c
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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#include <oblibs/stream.h>
#include <oblibs/io.h>
#include <oblibs/rbuffer.h>

/* Stack scratch for ostream_fmt(): formatted output up to this size avoids a
 * heap allocation. Larger output falls back to malloc -- no VLA (a runaway
 * format must not blow the stack). */
#define OSTREAM_FMT_STACK 1024


static uint8_t ostream_1_storage[OSTREAM_OUTSIZE] ;
ostream ostream_1_ = { { ostream_1_storage, OSTREAM_OUTSIZE, 0, 0 }, 1 } ;

static uint8_t ostream_2_storage[OSTREAM_OUTSIZE] ;
ostream ostream_2_ = { { ostream_2_storage, OSTREAM_OUTSIZE, 0, 0 }, 2 } ;

int ostream_init(ostream *o, int fd, void *storage, size_t size)
{
    if (!rbuffer_init(&o->rb, storage, size))
        return 0 ;
    o->fd = fd ;
    return 1 ;
}

int ostream_flush(ostream *o)
{
    struct iovec v[2] ;
    unsigned int n = rbuffer_riovec(&o->rb, v) ;
    if (!n)
        return 1 ;   // nothing buffered

    size_t total = rbuffer_used(&o->rb) ;
    size_t w = io_allwritev(o->fd, v, (int)n) ;
    rbuffer_rskip(&o->rb, w) ;
    return w == total ;
}

int ostream_put(ostream *o, char const *s, size_t len)
{
    if (!len)
        return 1 ;

    if (rbuffer_available(&o->rb) < len) {
        if (!ostream_flush(o))
            return 0 ;
        if (len >= rbuffer_capacity(&o->rb))   // too big to buffer: write through
            return io_allwrite(o->fd, (char *)s, len) == len ;
    }

    rbuffer_write(&o->rb, s, len) ;   // room is guaranteed here
    return 1 ;
}

int ostream_puts(ostream *o, char const *s)
{
    return ostream_put(o, s, strlen(s)) ;
}

int ostream_putv(ostream *o, struct iovec const *iov, int iovcnt)
{
    for (int i = 0 ; i < iovcnt ; i++)
        if (!ostream_put(o, iov[i].iov_base, iov[i].iov_len))
            return 0 ;
    return 1 ;
}

int ostream_putflush(ostream *o, char const *s, size_t len)
{
    return ostream_put(o, s, len) && ostream_flush(o) ;
}

int ostream_fmt(ostream *o, char const *fmt, ...)
{
    va_list ap, ap2 ;
    char small[OSTREAM_FMT_STACK] ;
    char *buf = small ;
    int n ;

    /* First try formats straight into the stack buffer: in the common case it
     * both measures and produces the output in a single vsnprintf. */
    va_start(ap, fmt) ;
    va_copy(ap2, ap) ;
    n = vsnprintf(small, sizeof small, fmt, ap) ;
    va_end(ap) ;
    if (n < 0) {
        va_end(ap2) ;
        return 0 ;
    }

    if ((size_t)n >= sizeof small) {     // didn't fit: format again on the heap
        buf = malloc((size_t)n + 1) ;
        if (!buf) {
            va_end(ap2) ;
            return 0 ;
        }
        vsnprintf(buf, (size_t)n + 1, fmt, ap2) ;
    }
    va_end(ap2) ;

    int r = ostream_put(o, buf, (size_t)n) ;
    if (buf != small)
        free(buf) ;
    return r ;
}

int ostream_cats(ostream *o, ...)
{
    va_list ap ;
    char const *s ;

    va_start(ap, o) ;
    while ((s = va_arg(ap, char const *))) {
        if (!ostream_put(o, s, strlen(s))) {
            va_end(ap) ;
            return 0 ;
        }
    }
    va_end(ap) ;
    return 1 ;
}

