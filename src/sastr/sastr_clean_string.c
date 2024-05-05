/*
 * sastr_clean_string.c
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
#include <oblibs/lexer.h>

#include <skalibs/stralloc.h>

int sastr_clean_string(stralloc *sa,char const *string)
{
    if (!*string) return (errno = EINVAL, 0) ;
    _alloc_stk_(stk, strlen(string) + 1) ;

    if (!stack_string_clean(&stk, string) ||
        !stralloc_copyb(sa, stk.s, stk.len) ||
        !stralloc_0(sa))
            return (errno = EOVERFLOW, 0) ;

    sa->len-- ;

    return 1 ;
}
