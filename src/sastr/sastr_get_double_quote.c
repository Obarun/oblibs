/*
 * sastr_get_double_quote.c
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

#include <oblibs/sastr.h>

#include <sys/types.h>

#include <oblibs/mill.h>

#include <skalibs/stralloc.h>

int sastr_get_double_quote(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    size_t pos = 0 ;
    stralloc kp = STRALLOC_ZERO ;
    wild_zero_all(&MILL_GET_DOUBLE_QUOTE) ;
    r = mill_element(&kp,sa->s,&MILL_GET_DOUBLE_QUOTE,&pos) ;
    if (r == -1 || !r) goto err ;
    if (!sastr_clean_element(&kp)) goto err ;
    if (kp.len) if (!sastr_rebuild_in_oneline(&kp)) goto err ;
    if (!stralloc_copy(sa,&kp)) goto err ;
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}
