/*
 * environ_clean_key.c
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

#include <oblibs/environ.h>

#include <stddef.h>

#include <oblibs/stack.h>
#include <oblibs/mill.h>
#include <oblibs/string.h>

int environ_clean_key(stack *stk)
{
    if (!stk->len ||
        get_sep_before(stk->s, '=', '\n') <= 0
        ) return 0 ;
    int r ;
    size_t newpos = 0 ;
    char tmp[stk->len + 1] ;
    auto_strings(tmp, stk->s) ;
    stk->len = stk->count = 0 ;
    wild_zero_all(&MILL_GET_KEY_NCLEAN) ;
    r = mill_element(stk, tmp, &MILL_GET_KEY_NCLEAN, &newpos) ;
    if (r == -1 || !r) return 0 ;
    if (!stack_close(stk)) return 0 ;

    return 1 ;
}