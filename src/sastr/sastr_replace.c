/*
 * sastr_replace.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
#include <oblibs/string.h>

#include <string.h>

#include <skalibs/stralloc.h>

int sastr_replace (stralloc *sa,char const *regex,char const *by)
{
    if (!sa->len || !*regex || !strcmp(regex,by)) return 0 ;
    size_t i = 0, j = 0, slen = strlen(regex) ;
    unsigned int found = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    stralloc shrink = STRALLOC_ZERO ;

    char ori[sa->len + 1] ;
    auto_strings(ori,sa->s) ;

    for (; i < sa->len; i++)
    {
        found = 0 ; j = 0 ;
        for (;j < slen;j++)
        {
            char c = regex[j] ;
            if (c != sa->s[i+j]){ found = 0 ; break ; }
            found++ ;
        }
        if (found){
            if (!auto_stra(&tmp,by)) goto err ;
            shrink.len = 0 ;
            if (!auto_stra(&shrink,ori + i + slen)) goto err ;
            if (shrink.len) // 0 mean end of line
            {
                if (!sastr_replace(&shrink,regex,by)) goto err ;
                if (!auto_stra(&tmp,shrink.s)) goto err ;
                break ;
            }
            if (!auto_stra(&tmp,ori + i + slen)) goto err ;
            break ;
        }
        if (!stralloc_catb(&tmp,ori+i,1)) goto err ;
        stralloc_0(&tmp) ;
        tmp.len-- ;
    }
    if (!stralloc_0(&tmp)) goto err ;
    tmp.len-- ;
    if (!stralloc_copy(sa,&tmp)) goto err ;

    stralloc_free(&tmp) ;
    stralloc_free(&shrink) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        stralloc_free(&shrink) ;
        return 0 ;
}
