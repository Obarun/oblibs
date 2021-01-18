/*
 * obstr_sub.c
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

#include <stddef.h>

#include <oblibs/memory.h>

char *obstr_sub(char const *str, unsigned int index, size_t len)
{
    unsigned int loop ;
    char *result ;

    if (!str)
        return NULL ;
    result = obstr_alloc(len+1) ;
    if (!result)
        return NULL ;
    loop = 0 ;
    while (loop < len)
    {
        result[loop] = str[index] ;
        loop++ ;
        index++ ;
    }
    result[index] = '\0' ;
    return result ;
}
