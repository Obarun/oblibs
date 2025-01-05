/*
 * sastr_replace_all.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>

int sastr_replace_g(stralloc *sa, const char *regex, const char *by)
{
    if (!sastr_split_string_in_nline(sa) ||
        !sastr_replace(sa, regex, by))
            return 0 ;

    return sastr_rebuild_in_nline(sa) ;
}
