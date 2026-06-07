/*
 * environ_rebuild.c
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <oblibs/sbl.h>
#include <oblibs/environ.h>

uint8_t environ_rebuild(strbuf *sb)
{
    if (!sb->len)
        return 1 ;
    size_t pos = 0 ;
    size_t n = sbl_count(sb) ;
    _alloc_strbuf_(s, sb->len + n + 1) ;

    FOREACH_SBL(sb, pos) {
        if (!strbuf_catb(&s, sb->s + pos, strlen(sb->s + pos)) ||
            !strbuf_catb(&s, "\n", 1))
                return 0 ;
    }
    s.s[s.len - 1] = 0 ;

    sb->len = 0 ;
    if (!strbuf_copyb(sb, s.s, s.len) ||
        !strbuf_terminate(sb))
            return 0 ;
    sb->len-- ;

    return  1 ;
}
