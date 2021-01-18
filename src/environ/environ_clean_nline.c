/*
 * environ_clean_nline.c
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

#include <oblibs/environ.h>

#include <string.h>

#include <oblibs/mill.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int environ_clean_nline(stralloc *sa)
{
    if (!sa->len) return 0 ;
    int r ;
    size_t pos = 0 ;
    stralloc cp = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    stralloc final = STRALLOC_ZERO ;
    wild_zero_all(&MILL_GET_LINE) ;
    r = mill_string(&cp,sa,&MILL_GET_LINE) ;
    if (r == -1 || !r) goto err ;
    for (;pos < cp.len; pos += strlen(cp.s + pos)+1)
    {
        tmp.len = 0 ;
        if (!stralloc_catb(&tmp,cp.s+pos,strlen(cp.s + pos)+1) ||
        !environ_clean_line(&tmp)) goto err ;
        if (!stralloc_0(&tmp)) goto err ;
        tmp.len-- ;
        if (!stralloc_catb(&final,tmp.s,strlen(tmp.s)) ||
        !stralloc_catb(&final,"\n",1)) goto err ;
    }
    final.len-- ;
    if (!stralloc_copy(sa,&final)) goto err ;
    stralloc_free(&cp) ;
    stralloc_free(&tmp) ;
    stralloc_free(&final) ;
    return 1 ;
    err:
        stralloc_free(&cp) ;
        stralloc_free(&tmp) ;
        stralloc_free(&final) ;
        return 0 ;
}
