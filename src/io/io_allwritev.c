/*
 * io_allwritev.c
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

#include <sys/types.h>
#include <sys/uio.h>

#include <oblibs/io.h>

size_t io_allwritev(int fd, struct iovec const *iov, int iovcnt)
{
    if (iovcnt <= 0)
        return 0 ;

    struct iovec vv[iovcnt] ;
    size_t total = 0, put = 0 ;
    int i = 0 ;
    for (; i < iovcnt ; i++) {
        vv[i] = iov[i] ;
        total += iov[i].iov_len ;
    }

    i = 0 ; // index of the first not-fully-written iovec
    while (put < total) {

        ssize_t r = io_write_result(io_writev(fd, vv + i, iovcnt - i)) ;
        if (r <= 0)
            break ;     // r < 0 : error ; r == 0 : would-block (out of contract)

        put += (size_t)r ;

        size_t adv = (size_t)r ;

        while (i < iovcnt && adv >= vv[i].iov_len) {
            adv -= vv[i].iov_len ;
            i++ ;
        }

        if (i < iovcnt && adv) {
            vv[i].iov_base = (char *)vv[i].iov_base + adv ;
            vv[i].iov_len -= adv ;
        }
    }

    return put ;
}
