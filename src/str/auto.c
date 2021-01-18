/*
 * auto.c
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

#include <errno.h>

#include <string.h>
#include <stdarg.h>

#include <skalibs/stralloc.h>

void auto_string_builder(char *dest,size_t baselen, char const *str[],...)
{
    int e = errno ;
    va_list alist ;
    va_start(alist,str) ;
    size_t slen = 0, totlen = baselen ;

    while (*str)
    {
        slen = strlen(*str) ;
        memcpy(dest + totlen,*str,slen) ;
        str++ ;
        totlen += slen ;
    }
    va_end(alist) ;
    errno = e ;
    dest[totlen] = 0 ;
}

int auto_stra_builder(stralloc *sa,char const *str[],...)
{
    int e = errno ;
    va_list alist ;
    va_start(alist,str) ;

    while (*str)
    {
        if (!stralloc_catb(sa,*str,strlen(*str))) return 0 ;
        str++ ;
    }
    va_end(alist) ;
    if(!stralloc_0(sa)) return 0 ;
    sa->len-- ;
    errno = e ;
    return 1 ;
}
