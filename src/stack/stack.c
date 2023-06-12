/*
 * stack.c
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

#include <stdint.h>
#include <string.h>

#include <oblibs/stack.h>
#include <oblibs/mill.h>

/**
 * \t\r\n are not escaped
 * */
uint8_t stack_convert_string(stack *stk, char const *string, size_t len)
{
    if (!len)
        return 0 ;
    int r ;
    _init_stack_(tmp, len + 1) ;
    wild_zero_all(&MILL_SPLIT_ELEMENT) ;
    r = mill_string(&tmp, string, len, &MILL_SPLIT_ELEMENT) ;
    if (r == -1 || !r) return 0 ;
    if (!stack_close(&tmp)) return 0 ;
    memmove(stk->s, tmp.s, tmp.len) ;
    stk->len = tmp.len ;
    if (!stack_close(stk)) return 0 ;
    return 1 ;
}

uint8_t stack_add(stack *stk, char const *str, size_t len)
{
    if (stk->len + len > stk->maxlen)
        return 0 ;
    memcpy(stk->s + stk->len, str, len) ;
    stk->len += len ;
    return 1 ;
}

uint8_t stack_add_g(stack *stk, char const *str)
{
    size_t len = strlen(str) ;
    return stack_add(stk, str, len) ;
}

uint8_t stack_close(stack *stk)
{
    if (stk->len + 1 > stk->maxlen)
        return 0 ;
    stack_add(stk, "", 1) ;
    stk->len-- ;
    return 1 ;
}
