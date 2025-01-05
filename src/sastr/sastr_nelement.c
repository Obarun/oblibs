/*
 * sastr_nelement.c
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

#include <oblibs/sastr.h>

#include <stddef.h>

#include <skalibs/stralloc.h>

size_t sastr_nelement(stralloc *sa)
{
    size_t n = 0, pos = 0 ;
    for (; pos < sa->len; pos += strlen(sa->s + pos) + 1)
        n++ ;

    return n ;
}
