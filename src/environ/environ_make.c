/*
 * environ_make.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * this following function come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libenvexec/env_make.c
 * under license ISC. All credits go to Laurent Bercot authors of these functions.
 * These functions was modified by Eric Vidal <eric@obarun.org> to terminate the array with NULL*/

#include <string.h>
#include <errno.h>
#include <oblibs/environ.h>

int environ_make(char const **v, size_t argc, char const *s, size_t len)
{
    while (argc--) {

        size_t n = strlen(s) + 1 ;
        if (n > len) return (errno = EINVAL, 0) ;
        *v++ = s ; s += n ; len -= n ;
    }

    v[argc] = 0 ;

    return 1 ;
}