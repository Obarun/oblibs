/*
 * environ_search_key.c
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

#include <string.h>
#include <errno.h>

#include <oblibs/lexer.h>
#include <oblibs/strbuf.h>

uint8_t environ_search_key(strbuf *sb, const char *str, const char *key, lexer_config *cfg)
{
    if (!*str || !*key)
        return (errno = EINVAL, 0) ;

     size_t slen = strlen(str) ;
    _alloc_strbuf_(s, slen + 2) ;

    if (!strbuf_catb(&s, "\n", 1) ||
        !strbuf_catb(&s, str, slen) ||
        !strbuf_terminate(&s))
            return (errno = EOVERFLOW, 0) ;
    s.len-- ; /* close: NUL at s.s[len], not counted (was stack_close) */

    cfg->str = s.s ; cfg->slen = s.len ;
    cfg->open = "\n"; cfg->olen = 1 ;
    cfg->close = "=" ; cfg->clen = 1 ;
    cfg->skip = " \t\r\n" ; cfg->skiplen = 4 ;
    cfg->forceclose = 1 ; cfg->firstoccurence = 1 ;
    cfg->style = 0 ; cfg->kopen = 0 ; cfg->kclose = 0 ;

    while (cfg->pos < slen) {

        cfg->opos = cfg->cpos = 0 ;
        lexer_reset(cfg) ;
        sb->len = 0 ;

        if (!lexer_trim(sb, cfg))
            return (errno = EINVAL, 0) ;

        if (cfg->found) {
            if (!strcmp(sb->s, key))
                return 1 ;
            cfg->pos = ++cfg->opos ;
        } else
            cfg->pos++ ;
    }

    lexer_reset(cfg) ;

    return (errno = EINVAL, 0) ;
}
