/*
 * sastr_remove_element.c
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

int sastr_remove_element(stralloc *sa,char const *element)
{
    stralloc tmp = STRALLOC_ZERO ;

    ssize_t el = sastr_find(sa,element) ;

    if (el == -1) return 1 ;

    size_t pos = 0 ;

    for (;pos < sa->len; pos += strlen(sa->s + pos) + 1)
    {
        if (pos == (size_t)el) continue ;
        if (!sastr_add_string(&tmp,sa->s + pos)) return 0 ;
    }

    if (!stralloc_copy(sa,&tmp)) return 0 ;

    stralloc_free(&tmp) ;

    return 1 ;
}


