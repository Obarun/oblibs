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