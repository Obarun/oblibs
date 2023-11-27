/*
 * str_start_with.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

int str_start_with(char const *str, char const *regex)
{
    if (!*str || !*regex) return 0 ;

    size_t i = 0, j = 0, len = strlen(str), slen = strlen(regex) ;

    for (; i < len && i < slen ; i++){

        for (;j < slen ; j++) {

            char c = regex[j] ;

            if (c != str[i+j])
                return 0 ;
        }
    }

    return 1 ;
}
