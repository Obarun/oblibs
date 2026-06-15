/*
 * get_rlen_until.c
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>
#include <string.h>

ssize_t get_rlen_until(const char *str, char const end, size_t from)
{
    int i ;
    size_t len,f ;
    len = strlen(str) ;
    f = len-from ;
    len = len-f ;
    char tmp[len + 1] ;
    memcpy(tmp,str,len) ;
    tmp[len] = 0 ;
    for (i = len; i >=0 ;i--)
        if (tmp[i] == end) return i ;

    return -1 ;
}
