/*
 * environ_clean_line.c
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

#include <oblibs/environ.h>
#include <oblibs/mill.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

int environ_clean_line(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    _init_stack_(stk, sa->len + 1) ;
    stralloc clean = STRALLOC_ZERO ;
    wild_zero_all(&MILL_CLEAN_SPLITTED_VALUE) ;
    if (!stralloc_copy(&clean,sa)) goto err ;
    if (!sastr_split_element_in_nline(&clean)) goto err ;
    r = mill_nstring(&stk, clean.s, clean.len, &MILL_CLEAN_SPLITTED_VALUE) ;
    if (r == -1 || !r) goto err ;
    if (!stack_close(&stk)) goto err ;
    clean.len = 0 ;
    if (!stralloc_copyb(&clean, stk.s, stk.len)) return 0 ;
    if (!environ_rebuild_line(&clean)) goto err ;
    if (!stralloc_copy(sa,&clean)) goto err ;
    stralloc_free(&clean) ;
    return 1 ;
    err:
        stralloc_free(&clean) ;
        return 0 ;
}
