/*
 * environ_get_commented_line.c
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

#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int environ_get_commented_line(stralloc *tokeep,stralloc *src)
{
    size_t pos = 0, len = src->len + 1, current = 0 ;
    char tmp[len] ;

    memset(tmp,0,len) ;

    if (!stralloc_copy(tokeep,src) ||
        !stralloc_0(tokeep))
            return 0 ;

    if (!sastr_split_string_in_nline(tokeep))
        return 0 ;

    FOREACH_SASTR(tokeep,pos) {

        if ((tokeep->s[pos] != '#') && (tokeep->s[pos] != ';'))
            continue ;

        current = strlen(tmp) ;

        auto_strings(tmp + current,tokeep->s + pos,"\n") ;
    }

    tokeep->len = 0 ;

    if (!auto_stra(tokeep,tmp))
        return 0 ;

    return 1 ;
}


