/*
 * environ_untrim.c
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

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

/** Exact inverse of environ_trim: take the NUL-separated key=value entries @sbl
 * (as environ_trim produces) and append the envfile text for them to @out, one
 * "key=<encoded value>\n" line per entry. Each value is re-encoded with
 * environ_untrim_value, so parsing the result back with environ_trim reproduces
 * @sbl. The key is appended verbatim (it is already clean in an sbl entry). */
int environ_untrim(strbuf *out, strbuf const *sbl)
{
    size_t i = 0 ;

    while (i < sbl->len) {

        char const *entry = sbl->s + i ;
        size_t elen = strlen(entry) ;
        ssize_t eq = get_len_until(entry, '=') ;

        if (eq < 0)
            return (errno = EINVAL, 0) ;

        _alloc_strbuf_(key, eq + 1) ;
        if (!strbuf_copyb(&key, entry, eq) || !strbuf_uncounted(&key))
            return 0 ;

        if (!environ_untrim_kv(out, key.s, entry + eq + 1))
            return 0 ;

        i += elen + 1 ;
    }

    return 1 ;
}
