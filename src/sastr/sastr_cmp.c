/*
 * sastr_cmp.c
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

#include <oblibs/sastr.h>

#include <string.h>

#include <skalibs/stralloc.h>

ssize_t sastr_cmp(stralloc *sa,char const *s)
{
    size_t i = 0, len = sa->len ;
    for (;i < len; i += strlen(sa->s + i) + 1)
        if (!strcmp(sa->s+i,s)) return i ;
    return -1 ;
}
