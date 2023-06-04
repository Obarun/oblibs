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

#include <string.h>

#include <oblibs/sastr.h>
#include <oblibs/mill.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int sastr_split_element_in_nline(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    _init_stack_(stk, sa->len + 1) ;
    wild_zero_all(&MILL_SPLIT_ELEMENT) ;
    r = mill_string(&stk, sa->s, sa->len, &MILL_SPLIT_ELEMENT) ;
    if (r == -1 || !r) return 0 ;
    if (!stack_close(&stk))
    sa->len = 0 ;
    if (!stralloc_copyb(sa, stk.s, stk.len)) return 0 ;
    return 1 ;
}
