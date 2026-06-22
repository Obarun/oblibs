/*
 * auto.c
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
#include <stdarg.h>

#include <oblibs/stream.h>
#include <oblibs/strbuf.h>

void auto_string_builder(char *dest,size_t baselen, char const *str[],...)
{
    va_list alist ;
    va_start(alist,str) ;
    size_t slen = 0, totlen = baselen ;

    while (*str)
    {
        slen = strlen(*str) ;
        memcpy(dest + totlen, *str, slen) ;
        str++ ;
        totlen += slen ;
    }
    va_end(alist) ;
    dest[totlen] = 0 ;
}

int auto_buf_builder(ostream *b, char const *str[], ...)
{
    va_list alist ;
    va_start(alist,str) ;

    while (*str) {

        if (!ostream_puts(b, *str)) {
            va_end(alist) ;
            return 0 ;
        }

        str++ ;
    }
    va_end(alist) ;
    return 1 ;
}

int auto_strbuf_builder(strbuf *b, char const *str[], ...)
{
    va_list alist ;
    va_start(alist,str) ;

    while (*str) {

        if (!strbuf_cats(b, *str)) {
            va_end(alist) ;
            return 0 ;
        }

        str++ ;
    }
    va_end(alist) ;
    if (!strbuf_uncounted(b))
        return 0 ;
    return 1 ;
}
