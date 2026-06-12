/*
 * file_tmpname.c
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
#include <sys/random.h>

#include <oblibs/files.h>

int file_tmpname(char *x, size_t n)
{
    static char const a[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_" ;
    unsigned char buf[n] ;
    if (getrandom(buf, n, 0) != (ssize_t)n) return 0 ;
    for (size_t i = 0 ; i < n ; i++) x[i] = a[buf[i] & 63] ;
    return 1 ;
}
