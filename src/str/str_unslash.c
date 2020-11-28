/*
 * str_unslash.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <oblibs/string.h>

void str_unslash (char *s)
{
    size_t l = strlen (s) ;

    if (l <= 1)
        return ;
    --l ;
    if (s[l] == '/')
        s[l] = '\0' ;
}
