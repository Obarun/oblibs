/*
 * strbuf.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>

#include <oblibs/strbuf.h>

int strbuf_reserve(strbuf *sb, size_t n)
{
    if (n <= sb->cap)
        return 1 ;

    size_t newcap = sb->cap + (sb->cap >> 1) ;        /* 1.5x */
    if (newcap < sb->cap || newcap < n)               /* overflow or still too small */
        newcap = n ;

    if (sb->heap) {
        char *p = realloc(sb->s, newcap) ;
        if (!p) return 0 ;
        sb->s = p ;
    } else {
        char *p = malloc(newcap) ;                    /* borrowed -> migrate to heap */
        if (!p) return 0 ;
        if (sb->len) memcpy(p, sb->s, sb->len) ;
        sb->s = p ;
        sb->heap = 1 ;
    }
    sb->cap = newcap ;
    return 1 ;
}

int strbuf_uncounted(strbuf *sb)
{
    if (!strbuf_reserve(sb, sb->len + 1))
        return 0 ;
    sb->s[sb->len] = 0 ;
    return 1 ;
}

int strbuf_with(strbuf *sb, void *storage, size_t size)
{
    if (!sb || !storage)
        return (errno = EINVAL, 0) ;
    sb->s = storage ;
    sb->len = 0 ;
    sb->cap = size ;
    sb->heap = 0 ;
    return 1 ;
}

int strbuf_copyb(strbuf *sb, char const *s, size_t n)
{
    if (!strbuf_reserve(sb, n))
        return 0 ;
    if (n) memmove(sb->s, s, n) ;
    sb->len = n ;
    return 1 ;
}

int strbuf_catb(strbuf *sb, char const *s, size_t n)
{
    size_t need = sb->len + n ;
    if (need < sb->len)
        return (errno = ERANGE, 0) ;
    if (!strbuf_reserve(sb, need))
        return 0 ;
    if (n) memmove(sb->s + sb->len, s, n) ;
    sb->len = need ;
    return 1 ;
}

int strbuf_catv(strbuf *sb, struct iovec const *v, unsigned int n)
{
    size_t total = 0 ;
    for (unsigned int i = 0 ; i < n ; i++) {
        if (total + v[i].iov_len < total)
            return (errno = ERANGE, 0) ;
        total += v[i].iov_len ;
    }
    size_t need = sb->len + total ;
    if (need < sb->len)
        return (errno = ERANGE, 0) ;
    if (!strbuf_reserve(sb, need))
        return 0 ;
    for (unsigned int i = 0 ; i < n ; i++) {
        if (v[i].iov_len) memmove(sb->s + sb->len, v[i].iov_base, v[i].iov_len) ;
        sb->len += v[i].iov_len ;
    }
    return 1 ;
}

int strbuf_insertb(strbuf *sb, size_t offset, char const *s, size_t n)
{
    if (offset > sb->len)
        return (errno = EINVAL, 0) ;
    size_t need = sb->len + n ;
    if (need < sb->len)
        return (errno = ERANGE, 0) ;
    if (!strbuf_reserve(sb, need))
        return 0 ;
    if (sb->len - offset)
        memmove(sb->s + offset + n, sb->s + offset, sb->len - offset) ;
    if (n)
        memmove(sb->s + offset, s, n) ;
    sb->len = need ;
    return 1 ;
}

int strbuf_shrink(strbuf *sb)
{
    if (!sb->heap)                     /* borrowed: nothing we own to shrink */
        return 1 ;
    if (sb->cap > sb->len) {
        size_t n = sb->len ? sb->len : 1 ;
        char *p = realloc(sb->s, n) ;
        if (!p) return 0 ;
        sb->s = p ;
        sb->cap = n ;
    }
    return 1 ;
}

void strbuf_free(strbuf *sb)
{
    if (sb->heap)
        free(sb->s) ;
    *sb = (strbuf)STRBUF_ZERO ;
}
