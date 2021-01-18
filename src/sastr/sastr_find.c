/*
 * sastr_find.c
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
#include <sys/types.h>

#include <skalibs/stralloc.h>

ssize_t sastr_find (stralloc *sa,char const *str)
{
    if (!sa->len) return -1 ;
    size_t i = 0, j = 0, slen = strlen(str) ;
    ssize_t found = 0 ;
    for (; i < sa->len; i++)
    {
        found = 0 ; j = 0 ;
        for (;j < slen;j++)
        {
            char c = str[j] ;
            if (c != sa->s[i+j]) {
                found = 0 ; break ;
            }
            found++ ;
        }
        if (found > 0) break ;
    }
    return found ? (ssize_t)i : -1 ;
}
