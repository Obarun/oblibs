/*
 * sastr_clean_string.c
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
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int sastr_clean_string(stralloc *sa,char const *string)
{
    if (!auto_stra(sa,string) ||
        !sastr_clean_element(sa))
            return 0 ;
    return 1 ;
}

int sastr_clean_string_flush_sa(stralloc *sa, char const *string)
{
    char tmp[sa->len + 1] ;

    auto_strings(tmp, sa->s) ;

    sa->len = 0 ;

    return sastr_clean_string(sa, tmp) ;
}
