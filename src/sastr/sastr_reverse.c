/*
 * sastr_reverse.c
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


int sastr_reverse(stralloc *sa)
{
    size_t pos = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    for (; pos < sa->len ; pos += strlen(sa->s + pos) + 1)
    {
        char *s = sa->s + pos ;
        if (!stralloc_insertb (&tmp,0,s,strlen(s)+1))
            goto err ;
    }
    if (!stralloc_copy(sa,&tmp)) goto err ;
    stralloc_free(&tmp) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
