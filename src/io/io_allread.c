/*
 * io_allread.c
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

#include <oblibs/io.h>

size_t io_allread(int fd, char *store, size_t len)
{
    size_t got = 0 ;
    while (got < len) {
        ssize_t r = io_read_result(io_read(fd, store + got, len - got)) ;
        if (r <= 0) break ;   // r<0 : EOF(EPIPE) or error ; r==0 : would-block (out of contract)
        got += (size_t)r ;
    }
    return got ;
}
