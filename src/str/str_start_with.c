/*
 * str_start_with.c
 *
 * Copyright (c) 2020 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>

int str_start_with(char const *str, char const *regex)
{
    size_t len = strlen(str), slen = strlen(regex) ;
    if (len < slen)
        return 1 ;
    return strncmp(str, regex, slen) ;
}
