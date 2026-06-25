/*
 * environ_merge_environ.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include <oblibs/sbl.h>
#include <oblibs/environ.h>

#include "environ-internal.h"

uint8_t environ_merge_environ(strbuf *modifs, strbuf *new)
{
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!modifs->len)
        return 1 ;

    if (!strbuf_catb(&sa, new->s, new->len))
        return (errno = EOVERFLOW, 0) ;

    _alloc_strbuf_(nval, new->len + 1) ;

    /**
     * For each @modifs entry, keep it only if its key is absent from @new
     * (@new takes precedence on a shared key). The lookup is done directly on
     * the @new sbl -- no \n-join, no re-decode.
     */
    FOREACH_SBL(modifs, pos) {

        char *line = modifs->s + pos ;
        _alloc_strbuf_(ekey, strlen(line) + 1) ;

        // get the @modifs environment key
        if (!environ_get_key(&ekey, line))
            return 0 ;

        // keep the @modifs entry only when @new has no such key
        if (!environ_sbl_value(&nval, new->s, new->len, ekey.s)) {
            if (!sbl_add(&sa, line))
                return 0 ;
        }
    }

    // flush the modifs environment with the new one
    if (!strbuf_copyb(modifs, sa.s, sa.len) ||
        !strbuf_uncounted(modifs))
            return (errno = EOVERFLOW, 0) ;

    return 1 ;
}
