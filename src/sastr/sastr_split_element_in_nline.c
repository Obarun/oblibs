/*
 * sastr_split_element_in_nline.c
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
#include <oblibs/mill.h>

#include <string.h>

#include <skalibs/stralloc.h>

int sastr_split_element_in_nline(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    wild_zero_all(&MILL_SPLIT_ELEMENT) ;
    stralloc tmp = STRALLOC_ZERO ;
    r = mill_string(&tmp,sa,&MILL_SPLIT_ELEMENT) ;
    if (r == -1 || !r) goto err ;
    if (!stralloc_copy(sa,&tmp)) goto err ;
    stralloc_free(&tmp) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
