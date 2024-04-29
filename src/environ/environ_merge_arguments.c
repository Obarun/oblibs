/*
 * environ_merge_arguments.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <errno.h>

#include <oblibs/sastr.h>
#include <skalibs/stralloc.h>

uint8_t environ_merge_arguments(stralloc *modifs, char const *const *environ)
{
    if (!environ || !environ[0])
        return 0 ;
    char **e = (char **)environ ;
    for(;*e;e++)
        if (!sastr_add_string(modifs, *e))
            return (errno = EOVERFLOW, 0) ;
    return 1 ;
}
