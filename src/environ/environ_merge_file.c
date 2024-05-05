/*
 * environ_merge_file.c
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

#include <string.h>
#include <errno.h>

#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>

uint8_t environ_merge_file(stralloc *modifs, const char *file)
{
    _init_stack_(stk, MAXENV - 2) ; // init stack add +2 to maxlen
    _alloc_sa_(sa) ;

    if (!environ_parse_file(&stk, file))
        return 0 ;

    if (!modifs->len) {
        /** no previous file, copy verbatim*/
        if (!stralloc_copyb(modifs, stk.s, stk.len) ||
            !stralloc_0(modifs))
                return (errno = EOVERFLOW, 0) ;

        modifs->len-- ;
        return 1 ;
    }

    _alloc_sa_(new) ;

    if (!stralloc_copyb(&new, stk.s, stk.len) ||
        !stralloc_0(&new))
            return (errno = EOVERFLOW, 0) ;
    new.len-- ;
    if (!environ_merge_environ(modifs, &new))
        return 0 ;

    return 1 ;
}
