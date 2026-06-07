/*
 * environ_clean_unexport.c
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
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

uint8_t environ_clean_unexport(strbuf *modifs)
{
    if (!modifs->len)
        return 1 ;

    size_t pos = 0 ;
    _alloc_strbuf_(stk, modifs->len + 1) ;

    FOREACH_SBL(modifs, pos) {

        char *line = modifs->s + pos ;
        size_t len = strlen(line) ;
        ssize_t r = get_len_until(line, '=') ;
        if (r < 0)
            return 0 ;

        if (line[r + 1] == VAR_UNEXPORT) {

            if (!strbuf_catb(&stk, line, ++r) ||
                !strbuf_catb(&stk, line + (r + 1), len - (r + 1)) ||
                !strbuf_terminate(&stk))
                    return 0 ;

        } else {

            if (!strbuf_catb(&stk, line, len) ||
                !strbuf_terminate(&stk))
                    return 0 ;
        }
    }

    modifs->len = 0 ;

    if (!strbuf_copyb(modifs, stk.s, stk.len) ||
        !strbuf_terminate(modifs))
            return (errno = EOVERFLOW, 0) ;
    modifs->len-- ;

    return 1 ;
}
