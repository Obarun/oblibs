/*
 * sbl_sort.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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

#include <oblibs/sbl.h>

int sbl_sort_cmp(strbuf *sb, int (*cmp)(char const *a, char const *b))
{
    if (!sb->len)
        return 0 ;

    size_t nel = sbl_count(sb), idx = 0, a = 0, b = 0, pos = 0 ;
    /* Sort an array of pointers INTO sb->s: no per-element length cap, and the
     * stack cost is nel pointers instead of nel * 4096 bytes. */
    char const *names[nel] ;

    for (; pos < sb->len && idx < nel ; pos += strlen(sb->s + pos) + 1, idx++)
        names[idx] = sb->s + pos ;

    for (a = 0 ; a + 1 < nel ; a++)
        for (b = a + 1 ; b < idx ; b++)
            if (cmp(names[a], names[b]) > 0) {
                char const *t = names[a] ;
                names[a] = names[b] ;
                names[b] = t ;
            }

    /* names[] alias sb->s, so build the sorted list in a scratch buffer and copy
     * it back rather than overwriting sb->s while still reading from it. */
    _alloc_strbuf_(tmp, sb->len) ;
    for (a = 0 ; a < nel ; a++)
        if (!sbl_add(&tmp, names[a]))
            return 0 ;

    return strbuf_copy(sb, &tmp) ;
}

int sbl_sort(strbuf *sb)
{
    return sbl_sort_cmp(sb, strcmp) ;
}

int sbl_drop_duplicate(strbuf *sb)
{
    /* result only ever drops elements, so it fits in sb->len; non-recursive */
    _alloc_strbuf_(tmp, sb->len) ;
    size_t pos = 0 ;
    char const *prev = 0 ;

    FOREACH_SBL(sb, pos) {
        char const *cur = sb->s + pos ;
        if (prev && !strcmp(prev, cur))
            continue ;
        if (!sbl_add(&tmp, cur))
            return 0 ;
        prev = cur ;
    }

    return strbuf_copy(sb, &tmp) ;
}

int sbl_sortndrop(strbuf *sb)
{
    if (!sbl_sort(sb))
        return 0 ;
    return sbl_drop_duplicate(sb) ;
}
