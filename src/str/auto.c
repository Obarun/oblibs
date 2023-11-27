/*
 * auto.c
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

#include <errno.h>

#include <string.h>
#include <stdarg.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>

void auto_string_builder(char *dest,size_t baselen, char const *str[],...)
{
    va_list alist ;
    va_start(alist,str) ;
    size_t slen = 0, totlen = baselen ;

    while (*str)
    {
        slen = strlen(*str) ;
        char t[slen + 1] ;
        memcpy(t,*str,slen) ;
        t[slen] = 0 ;
        memcpy(dest + totlen,t,slen) ;
        str++ ;
        totlen += slen ;
    }
    va_end(alist) ;
    dest[totlen] = 0 ;
}

int auto_stra_builder(stralloc *sa,char const *str[],...)
{
    size_t slen = 0 ;
    va_list alist ;
    va_start(alist,str) ;

    while (*str) {

        slen =  strlen(*str) ;
        char t[slen + 1] ;
        memcpy(t, *str, slen) ;
        t[slen] = 0 ;

        if (!stralloc_catb(sa,*str,strlen(*str))) return 0 ;
        str++ ;
    }
    va_end(alist) ;
    if (!stralloc_0(sa)) return 0 ;
    sa->len-- ;
    return 1 ;
}

int auto_buf_builder(buffer *b, char const *str[], ...)
{
    size_t slen = 0 ;
    va_list alist ;
    va_start(alist,str) ;

    while (*str) {

        slen =  strlen(*str) ;
        char t[slen + 1] ;
        memcpy(t, *str, slen) ;
        t[slen] = 0 ;
        if (buffer_puts(b, *str) < 0)
            return 0 ;

        str++ ;
    }
    va_end(alist) ;
    return 1 ;
}
