/*
 * environ_get_key.c
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
#include <string.h>

#include <oblibs/lexer.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

uint8_t environ_get_key(strbuf *sb, const char *str)
{
    lexer_config cfg = LEXER_CONFIG_ZERO ;
    size_t slen = strlen(str) ;
    cfg.str = str ; cfg.slen = slen ;
    cfg.close = "=" ; cfg.clen = 1 ;
    cfg.skip = " \t\r\n" ; cfg.skiplen = 4 ;
    cfg.forceclose = 0 ; cfg.forceopen = 1 ; cfg.firstoccurence = 1 ;
    cfg.style = 0 ; cfg.kopen = 1 ; cfg.kclose = 0 ;

    if (!lexer_trim(sb, &cfg))
        return 0 ;

    return 1 ;
}
