/*
 * sastr_remove_element.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int sastr_remove_element(stralloc *sa, char const *element)
{
    if (!sa->len)
        return (errno = EINVAL) ;

    ssize_t index = sastr_cmp(sa, element) ;

    if (index == -1)
        return 1 ;

    size_t ilen = strlen(sa->s + index), next = index + ilen + 1, nextlen = strlen(sa->s + next) ;
    if (nextlen)
        memmove(sa->s + index, sa->s + next, sa->len - next) ;
    sa->len -= ilen + 1 ;
    if (!stralloc_0(sa)) return 0 ;
    sa->len-- ;

    return 1 ;
}


