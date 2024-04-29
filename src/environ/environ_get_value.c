/*
 * environ_get_value.c
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
#include <errno.h>

#include <oblibs/stack.h>
#include <oblibs/environ.h>
#include <oblibs/string.h>

uint8_t environ_get_value(stack *stk, const char *str)
{
    stack_reset(stk) ;
    if (!environ_trim(stk, str))
        return (errno = EINVAL, 0) ;
    ssize_t r = get_len_until(stk->s, '=') ;
    /** environ_trim parse the @str,
     * only keep the first value */
    size_t len = strlen(stk->s) - (++r) ;
    memmove(stk->s, stk->s + r, len) ;
    stk->s[len] = 0 ;
    stk->len = len ;
    stk->count = 1 ;
    return 1 ;
}