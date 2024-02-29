/*
 * environ_get_val_of_key.c
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
#include <oblibs/string.h>

static int environ_clean_key(stack *stk)
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

static int environ_get_key(stack *stkkey, char const *str, char const *key)
{
    int res = -1 ;
    size_t pos = 0, len = strlen(str) ;
    _init_stack_(stk, len + 1) ;

    if (!stack_copy(&stk, str, len))
        return res ;

    if (!stack_split_string_in_nline(&stk))
        return res ;

    FOREACH_STK(&stk, pos) {

        size_t l = strlen(stk.s + pos) ;
        stkkey->len = stkkey->count = 0 ;

        if (!stack_copy(stkkey, stk.s + pos, l) ||
            !stack_close(stkkey)) return res ;

        if (!environ_clean_key(stkkey))
            continue ;

        if (!strcmp(stkkey->s, key)) {
            res = pos ;
            break ;
        }
    }

    return res ;
}

int environ_get_val_of_key(stralloc *sa,char const *key)
{
    if (!sa->len) return 0 ;
    ssize_t pos = -1 ;
    _init_stack_(stkkey, sa->len + 1) ;
    pos = environ_get_key(&stkkey, sa->s, key) ;
    if (pos < 0)
        return 0 ;

    if (!environ_get_val(sa,(size_t *)&pos))
        return 0 ;

    return 1 ;
}
