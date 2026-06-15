/*
 * environ_get_val.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <string.h>
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>
#include <oblibs/environ.h>

uint8_t environ_search_value(strbuf *sb, const char *str, const char *key)
{
    lexer_config cfg = LEXER_CONFIG_ZERO ;
    _alloc_strbuf_(s, strlen(str) + 1) ;
    _alloc_strbuf_(t, strlen(str) + 1) ;

    if (!environ_search_key(&s, str, key, &cfg))
        return (errno = EINVAL, 0) ;

    /** @str is raw here: parse it once (the matched entry comes first from
     * cfg.opos), then extract its value. environ_get_value itself no longer
     * parses, so the trim must happen here. */
    if (!environ_trim(&t, str + cfg.opos))
        return 0 ;

    if (!environ_get_value(sb, t.s))
        return 0 ;

    return 1 ;
}
