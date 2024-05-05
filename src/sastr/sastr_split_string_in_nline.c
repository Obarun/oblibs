/*
 * sastr_split_string_in_nline.c
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

#include <errno.h>

#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <skalibs/stralloc.h>

int sastr_split_string_in_nline(stralloc *sa)
{
    if (!sa->len) return 0 ;
    _alloc_stk_(stk, sa->len + 1) ;
    if (!stack_string_trimline(&stk, sa->s))
        return 0 ;

    if (!stralloc_copyb(sa, stk.s, stk.len) ||
        !stralloc_0(sa))
            return (errno = EOVERFLOW, 0 ) ;

    sa->len-- ;

    return 1 ;
}
