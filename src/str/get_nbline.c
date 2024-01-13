/*
 * get_nbline.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/string.h>

unsigned int get_nbline(char const *str, size_t len)
{
    unsigned int pos, loop ;
    ssize_t r = 0 ;
    pos = loop = 0 ;


    while ((pos < len) && (r != -1))
    {
        r = get_len_until(str+pos,'\n') ;
        pos = r+pos+1 ;//+1 to skip the \n character
        loop++ ;
    }

    return loop ;
}
