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

#include <oblibs/sastr.h>

#include <string.h>

#include <skalibs/stralloc.h>

int sastr_remove_element(stralloc *sa, char const *element)
{
    ssize_t el = sastr_cmp(sa, element) ;

    if (el == -1)
        return 0 ;

    size_t len = sa->len, pos = 0 ;

    char t[len + 1] ;

    sastr_to_char(t, sa) ;

    sa->len = 0 ;

    for (;pos < len; pos += strlen(t + pos) + 1) {

        if (pos == (size_t)el)
            continue ;

        if (!sastr_add_string(sa, t + pos))
            return 0 ;
    }

    return 1 ;
}


