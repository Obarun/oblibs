/*
 * environ_get_val.c
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
#include <oblibs/stack.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int environ_get_val(stralloc *sa,size_t *pos)
{
    if (!sa->len) return 0 ;
    int r ;
    size_t newpos = 0 ;
    _init_stack_(stk, sa->len + 1) ;
    wild_zero_all(&MILL_GET_VAL) ;
    r = mill_element(&stk, sa->s + *pos, &MILL_GET_VAL, &newpos) ;
    if (r == -1 || !r) return 0 ;
    if (!stack_close(&stk)) return 0 ;
    sa->len = 0 ;
    if (!stralloc_copyb(sa, stk.s, stk.len)) return 0 ;
    *pos += newpos ;
    return 1 ;
}
