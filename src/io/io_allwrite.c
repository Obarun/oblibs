/*
 * io_allwrite.c
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

#include <oblibs/io.h>

size_t io_allwrite(int fd, char *store, size_t len)
{
    size_t put = 0 ;
    while (put < len) {
        ssize_t r = io_write_result(io_write(fd, store + put, len - put)) ;
        if (r <= 0) break ;   // r<0 : error ; r==0 : would-block (out of contract)
        put += (size_t)r ;
    }
    return put ;
}
