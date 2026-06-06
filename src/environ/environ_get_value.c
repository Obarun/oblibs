/*
 * environ_get_value.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/string.h>

/** Pure extraction: @str is expected to be an already-parsed "key=value"
 * entry (as produced by environ_trim upstream). Copy everything after the
 * first '=' up to '\n' or EOF into @stk verbatim. No re-parsing: running
 * environ_trim here would double-unescape (or reject) a value that already
 * went through the parser once. */
uint8_t environ_get_value(strbuf *sb, const char *str)
{
    ssize_t r = get_len_until(str, '=') ;
    if (r < 0)
        return (errno = EINVAL, 0) ;

    char const *val = str + r + 1 ;
    size_t len = 0 ;
    while (val[len] && val[len] != '\n')
        len++ ;

    if (!strbuf_reserve(sb, len + 1))
        return 0 ;

    memmove(sb->s, val, len) ;
    sb->s[len] = 0 ;
    sb->len = len ;
    return 1 ;
}