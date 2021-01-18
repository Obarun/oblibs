/*
 * sastr_add_string.c
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

int sastr_add_string(stralloc *sa,char const *str)
{
    if (!*str) return 0 ;
    size_t len = strlen(str) + 1 ;
    if (!stralloc_catb(sa,str,len)) return 0 ;
    return 1 ;
}
