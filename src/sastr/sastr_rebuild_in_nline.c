/*
 * sastr_rebuild_in_nline.c
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

#include <skalibs/stralloc.h>

int sastr_rebuild_in_nline(stralloc *sa)
{
    if (!sa->len) return 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    size_t i = 0, len = sa->len ;
    for (;i < len; i += strlen(sa->s + i) + 1)
    {
        if (!stralloc_catb(&tmp,sa->s+i,strlen(sa->s + i)) ||
        !stralloc_cats(&tmp,"\n")) goto err ;
    }
    //tmp.len-- ; //remove last '\n'
    if (!stralloc_copy(sa,&tmp) ||
        !stralloc_0(sa)) goto err ;
    sa->len-- ;
    stralloc_free(&tmp) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
