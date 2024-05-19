/*
 * environ_import_arguments.c
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

uint8_t environ_import_arguments(stralloc *modifs, char const *const *environ, size_t len)
{
    if (!environ || !environ[0])
        return (errno = EINVAL, 0) ;
    size_t pos = 0 ;
    for(; pos < len ; pos++) {
        if (!stralloc_cats(modifs, environ[pos]) || !stralloc_0(modifs))
            return 0 ;
    }

    return 1 ;
}
