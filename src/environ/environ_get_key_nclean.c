/*
 * environ_get_key_nclean.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/environ.h>

#include <string.h>
#include <oblibs/mill.h>

#include <skalibs/stralloc.h>

int environ_get_key_nclean(stralloc *sa,size_t *pos)
{
    if (!sa->len) return 0 ;
    int r ;
    size_t newpos = 0 ;
    stralloc kp = STRALLOC_ZERO ;
    wild_zero_all(&MILL_GET_KEY_NCLEAN) ;
    r = mill_element(&kp,sa->s + *pos,&MILL_GET_KEY_NCLEAN,&newpos) ;
    if (r == -1 || !r) goto err ;
    if (!stralloc_copy(sa,&kp) ||
        !stralloc_0(sa)) goto err ;
    sa->len-- ;
    *pos += newpos ;
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}
