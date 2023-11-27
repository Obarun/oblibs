/*
 * sastr_sort.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int sastr_sort(stralloc *sa)
{
    if (!sa->len) return 0 ;
    size_t salen = sa->len, nel = sastr_len(sa), idx = 0, a = 0, b = 0, pos = 0 ;
    char names[nel][4096] ;
    char tmp[4096] ;

    for (; pos < salen && idx < nel ; pos += strlen(sa->s + pos) + 1,idx++)
    {
        memcpy(names[idx],sa->s + pos,strlen(sa->s + pos)) ;
        names[idx][strlen(sa->s+pos)] = 0 ;
    }

    for (; a < nel - 1 ; a++)
    {
        for (b = a + 1 ; b < idx ; b++)
        {
            if (strcmp(names[a],names[b]) > 0)
            {
                auto_strings(tmp,names[a]) ;
                auto_strings(names[a],names[b]) ;
                auto_strings(names[b],tmp) ;
            }
        }
    }

    sa->len = 0 ;

    for (a = 0 ; a < nel ; a++)
        if (!sastr_add_string(sa,names[a]))
            return 0 ;

    return 1 ;
}
