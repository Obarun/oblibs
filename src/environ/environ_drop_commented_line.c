/*
 * environ_drop_commented_line.c
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

int environ_drop_commented_line(stralloc *sa)
{
    size_t pos = 0, len = sa->len + 1, current = 0 ;
    char tmp[len] ;

    memset(tmp,0,len * sizeof(char)) ;

    if (!sastr_split_string_in_nline(sa))
        return 0 ;

    FOREACH_SASTR(sa,pos) {

        if ((sa->s[pos] == '#') || (sa->s[pos] == ';'))
            continue ;

        current = !pos ? 0 : strlen(tmp) ;

        auto_strings(tmp + current,sa->s + pos,"\n") ;
    }

    sa->len = 0 ;

    if (!auto_stra(sa,tmp))
        return 0 ;

    return 1 ;
}


