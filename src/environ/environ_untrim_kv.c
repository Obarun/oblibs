/*
 * environ_untrim_kv.c
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

#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

/** Append "<key>=<encoded value>\n" to @out. The value goes through
 * environ_untrim_value so it decodes back byte-for-byte; @key is appended
 * verbatim and is the caller's responsibility (it must be a valid envfile key:
 * no '=', no '\n', no surrounding whitespace, and not start with '#'). */
uint8_t environ_untrim_kv(strbuf *out, char const *key, char const *value)
{
    if (!strbuf_cats(out, key))
        return 0 ;
    if (!strbuf_catb(out, "=", 1))
        return 0 ;
    if (!environ_untrim_value(out, value))
        return 0 ;
    return strbuf_catb(out, "\n", 1) ;
}
