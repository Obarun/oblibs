/*
 * environ_get_clean_env.c
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
#include <skalibs/types.h>

int environ_get_clean_env(stralloc *sa)
{
    size_t pos = 0 ;

    if (!sastr_split_string_in_nline(sa))
        return 0 ;

    /** empty file? */
    if (!sa->len)
        return 1 ;

    if (!environ_clean_nline(sa) ||
        !stralloc_0(sa))
            return 0 ;

    if (!environ_drop_commented_line(sa))
         return 0 ;

    /** may be badly formated */
    FOREACH_SASTR(sa,pos)
        if (!environ_keyvalue_isvalid(sa->s + pos))
            return 0 ;

    return 1 ;
}
