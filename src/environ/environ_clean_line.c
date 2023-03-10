/*
 * environ_clean_line.c
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int environ_clean_line(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    stralloc split = STRALLOC_ZERO ;
    stralloc clean = STRALLOC_ZERO ;
    wild_zero_all(&MILL_SPLIT_ELEMENT) ;
    wild_zero_all(&MILL_CLEAN_SPLITTED_VALUE) ;
    if (!stralloc_copy(&split,sa)) goto err ;
    if (!sastr_split_element_in_nline(&split)) goto err ;
    r = mill_nstring(&clean,&split,&MILL_CLEAN_SPLITTED_VALUE) ;
    if (r == -1 || !r) goto err ;
    if (!environ_rebuild_line(&clean)) goto err ;
    if (!stralloc_copy(sa,&clean) ||
        !stralloc_0(sa)) goto err ;
    sa->len-- ;
    stralloc_free(&split) ;
    stralloc_free(&clean) ;
    return 1 ;
    err:
        stralloc_free(&split) ;
        stralloc_free(&clean) ;
        return 0 ;
}
