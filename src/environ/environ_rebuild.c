/*
 * environ_rebuild.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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

#include <oblibs/sastr.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

uint8_t environ_rebuild(stralloc *sa)
{
    if (!sa->len)
        return 1 ;
    size_t pos = 0 ;
    size_t n = sastr_nelement(sa) ;
    _alloc_stk_(s, sa->len + n + 1) ;

    FOREACH_SASTR(sa, pos) {
        if (!stack_add(&s, sa->s + pos, strlen(sa->s + pos)) ||
            !stack_add(&s, "\n", 1))
                return 0 ;
    }
    s.s[s.len - 1] = 0 ;

    sa->len = 0 ;
    if (!stralloc_copyb(sa, s.s, s.len) ||
        !stralloc_0(sa))
            return 0 ;
    sa->len-- ;

    return  1 ;
}
