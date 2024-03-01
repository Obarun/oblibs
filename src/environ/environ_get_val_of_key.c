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

#include <stddef.h>

#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

int environ_get_val_of_key(stralloc *sa,char const *key)
{
    if (!sa->len) return 0 ;
    ssize_t pos = -1 ;
    _init_stack_(stk, sa->len + 1) ;
    pos = environ_get_key(&stk, sa->s, key) ;
    if (pos < 0)
        return 0 ;

    if (!environ_get_val(sa,(size_t *)&pos))
        return 0 ;

    return 1 ;
}
