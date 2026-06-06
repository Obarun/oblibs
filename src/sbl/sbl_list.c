/*
 * sbl_list.c
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

#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

int sbl_addb(strbuf *sb, char const *s, size_t n)
{
    if (!n)
        return (errno = EINVAL, 0) ;
    /* copy exactly @n bytes from @s -- no strlen, no over-read if @s is not
     * NUL-terminated -- then append the element's terminating NUL. */
    if (!strbuf_catb(sb, s, n))
        return 0 ;
    return strbuf_terminate(sb) ;
}

size_t sbl_count(strbuf *sb)
{
    size_t n = 0, pos = 0 ;
    FOREACH_SBL(sb, pos)
        n++ ;
    return n ;
}

ssize_t sbl_search(strbuf *sb, char const *s)
{
    size_t pos = 0 ;
    FOREACH_SBL(sb, pos)
        if (!strcmp(sb->s + pos, s))
            return (ssize_t)pos ;
    return -1 ;
}

ssize_t sbl_find(strbuf *sb, char const *str)
{
    if (!sb->len)
        return -1 ;
    size_t i = 0, j = 0, slen = strlen(str) ;
    ssize_t found = 0 ;
    for (; i < sb->len ; i++) {
        found = 0 ; j = 0 ;
        for (; j < slen ; j++) {
            if (str[j] != sb->s[i + j]) {
                found = 0 ; break ;
            }
            found++ ;
        }
        if (found > 0) break ;
    }
    return found ? (ssize_t)i : -1 ;
}

ssize_t sbl_element_byid(strbuf *sb, unsigned int id)
{
    size_t pos = 0, idx = 0 ;
    for (; pos < sb->len ; pos += strlen(sb->s + pos) + 1, idx++)
        if (idx == id)
            return (ssize_t)pos ;
    return -1 ;
}

int sbl_insert(strbuf *sb, size_t offset, char const *s)
{
    if (!*s)
        return (errno = EINVAL, 0) ;
    return strbuf_insertb(sb, offset, s, strlen(s)) ;
}

int sbl_remove_at(strbuf *sb, size_t offset)
{
    if (!sb->len || offset >= sb->len)
        return (errno = EINVAL, 0) ;

    size_t ilen = strlen(sb->s + offset) ;
    size_t next = offset + ilen + 1 ;
    /* Unlike stack, a strbuf gives no guaranteed NUL at s[len], so never read
     * past len: there is nothing to shift when the removed element is last. */
    if (next < sb->len)
        memmove(sb->s + offset, sb->s + next, sb->len - next) ;
    sb->len -= ilen + 1 ;
    return 1 ;
}

int sbl_remove(strbuf *sb, char const *s)
{
    ssize_t idx = sbl_search(sb, s) ;
    return idx < 0 ? 1 : sbl_remove_at(sb, (size_t)idx) ;
}

int sbl_reverse(strbuf *sb)
{
    /* reversed list has the exact same size as sb; non-recursive */
    _alloc_strbuf_(tmp, sb->len) ;
    size_t pos = 0 ;
    FOREACH_SBL(sb, pos) {
        char const *s = sb->s + pos ;
        if (!strbuf_insertb(&tmp, 0, s, strlen(s) + 1))
            return 0 ;
    }
    return strbuf_copy(sb, &tmp) ;
}

void sbl_to_char(char *dst, strbuf *sb)
{
    if (sb->len)
        memcpy(dst, sb->s, sb->len) ;
}
