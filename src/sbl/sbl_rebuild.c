/*
 * sbl_rebuild.c
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

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

/* Replace the list with its elements joined by @sep (one byte). When @keeplast
 * is 0 the trailing separator is dropped. The result is a single C string whose
 * len excludes its terminating NUL (a NUL is still written at s[len]). */
static int rebuild(strbuf *sb, char sep, int keeplast)
{
    if (!sb->len)
        return 0 ;

    _cleanup_strbuf_ strbuf tmp = STRBUF_ZERO ;

    size_t pos = 0 ;
    FOREACH_SBL(sb, pos) {
        if (!strbuf_catb(&tmp, sb->s + pos, strlen(sb->s + pos)) ||
            !strbuf_catb(&tmp, &sep, 1))
            return 0 ;
    }

    if (!keeplast && tmp.len)
        tmp.len-- ; /* drop the trailing separator */

    if (!strbuf_copy(sb, &tmp))
        return 0 ;

    if (!strbuf_uncounted(sb))
        return 0 ;

    return 1 ;
}

int sbl_rebuild_oneline(strbuf *sb)
{
    return rebuild(sb, ' ', 0) ;
}

int sbl_rebuild_nline(strbuf *sb)
{
    return rebuild(sb, '\n', 1) ;
}

int sbl_rebuild_with_delim(strbuf *sb, unsigned int delim)
{
    return rebuild(sb, (char)delim, 0) ;
}
