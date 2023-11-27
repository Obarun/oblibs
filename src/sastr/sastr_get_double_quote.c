/*
 * sastr_get_double_quote.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>

#include <oblibs/mill.h>
#include <oblibs/stack.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int sastr_get_double_quote(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    size_t pos = 0 ;
    _init_stack_(stk, sa->len + 1) ;
    wild_zero_all(&MILL_GET_DOUBLE_QUOTE) ;
    r = mill_element(&stk, sa->s, &MILL_GET_DOUBLE_QUOTE, &pos) ;
    if (r == -1 || !r) return 0 ;
    if (!stack_close(&stk)) return 0 ;
    sa->len = 0 ;
    if (!stralloc_copyb(sa, stk.s, stk.len)) return 0 ;
    if (!sastr_clean_element(sa)) return 0 ;
    if (sa->len) if (!sastr_rebuild_in_oneline(sa)) return 0 ;
    return 1 ;
}
