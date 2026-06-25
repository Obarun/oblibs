/*
 * environ_sbl_value.c
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

#include "environ-internal.h"

uint8_t environ_sbl_value(strbuf *out, char const *s, size_t len, char const *key)
{
    size_t klen = strlen(key) ;
    size_t i = 0 ;

    while (i < len) {

        char const *e = s + i ;
        size_t elen = strlen(e) ;
        ssize_t r = get_len_until(e, '=') ;

        /* exact key match on the part before the first '=' (r == klen guards
         * against a prefix collision such as key "FOO" vs entry "FOOBAR="). A
         * malformed entry with no '=' (r < 0) is skipped, not an error. */
        if (r >= 0 && (size_t)r == klen && !strncmp(e, key, klen))
            return environ_get_value(out, e) ;

        i += elen + 1 ;
    }

    return (errno = EINVAL, 0) ;
}
