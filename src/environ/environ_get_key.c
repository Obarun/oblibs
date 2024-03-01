/*
 * environ_add_key_val.c
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

#include <string.h>

#include <oblibs/stack.h>
#include <oblibs/mill.h>

int environ_get_key(stack *stk, char const *str, char const *key)
{
    int res = -1 ;
    size_t pos = 0, len = strlen(str) ;
    _init_stack_(s, len + 1) ;

    if (!stack_copy(&s, str, len) ||
        !stack_close(&s))
        return res ;

    if (!stack_split_string_in_nline(&s))
        return res ;

    FOREACH_STK(&s, pos) {

        size_t l = strlen(s.s + pos) ;
        stk->len = stk->count = 0 ;

        if (!stack_copy(stk, s.s + pos, l) ||
            !stack_close(stk)) return res ;

        if (!environ_clean_key(stk))
            continue ;

        if (!strcmp(stk->s, key)) {
            res = pos ;
            break ;
        }
    }

    return res ;
}
