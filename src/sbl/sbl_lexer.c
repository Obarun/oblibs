/*
 * sbl_lexer.c
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
 *
 * sbl operations that build a list through the lexer. They sit ABOVE the lexer
 * (which only knows strbuf), so the dependency stays acyclic: strbuf <- lexer
 * <- sbl.
 */

#include <string.h>
#include <errno.h>

#include <oblibs/lexer.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>

int sbl_clean_string(strbuf *sb, char const *string)
{
    if (!*string)
        return (errno = EINVAL, 0) ;
    /* replace semantics: @string must not alias sb->s */
    sb->len = 0 ;
    return lexer_trim_clean(sb, string) ;
}

int sbl_split_string_in_nline(strbuf *sb)
{
    if (!sb->len)
        return 0 ;
    /* sb->s is read as one C string; trim into a temp to avoid aliasing.
     * Not recursive: a stack seed sized to sb->len keeps small inputs off heap. */
    _alloc_strbuf_(tmp, sb->len) ;
    if (!lexer_trim_line(&tmp, sb->s))
        return 0 ;
    return strbuf_copy(sb, &tmp) ;
}

int sbl_replace(strbuf *sb, char const *regex, char const *by)
{
    if (!sb->len || !regex || !by || !strcmp(regex, by))
        return (errno = EINVAL, 0) ;

    size_t pos = 0, bylen = strlen(by) ;
    lexer_config cfg = LEXER_CONFIG_ZERO ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf new = STRBUF_ZERO ;

    cfg.open = (char *)regex ; cfg.olen = strlen(regex) ;
    cfg.close = "" ; cfg.clen = 1 ; cfg.style = 1 ; cfg.kopen = 0 ; cfg.kclose = 0 ;
    cfg.forceclose = 1 ; cfg.firstoccurence = 1 ;

    FOREACH_SBL(sb, pos) {

        stk.len = 0 ;
        lexer_reset_hard(&cfg) ;
        cfg.str = sb->s + pos ; cfg.slen = strlen(sb->s + pos) ;

        /* stk holds the run after the first @regex; only stk.len is read. */
        if (!lexer(&stk, &cfg))
            return 0 ;

        if (cfg.found) {

            _cleanup_strbuf_ strbuf line = STRBUF_ZERO ;

            if (cfg.opos)
                if (!strbuf_catb(&line, cfg.str, cfg.opos))
                    return 0 ;

            /* line = prefix + by, kept NUL-terminated for the sbl_add path. */
            if (!strbuf_catb(&line, by, bylen) ||
                !strbuf_uncounted(&line))
                    return 0 ;

            if (stk.len) {
                /** the rest of the line may contain @regex */
                _cleanup_strbuf_ strbuf c = STRBUF_ZERO ;
                if (!strbuf_copyb(&c, new.s, new.len))
                    return 0 ;

                new.len = 0 ;

                if (!strbuf_copyb(&new, stk.s, stk.len) ||
                    !strbuf_uncounted(&new))
                        return 0 ;

                if (!sbl_replace(&new, regex, by) ||
                    !strbuf_insertb(&new, 0, line.s, line.len) ||
                    !strbuf_insertb(&new, 0, c.s, c.len))
                        return 0 ;

            } else if (!sbl_addb(&new, line.s, line.len))
                return 0 ;

        } else if (!sbl_addb(&new, cfg.str, cfg.slen))
            return 0 ;
    }

    return strbuf_copy(sb, &new) ;
}

int sbl_replace_nline(strbuf *sb, char const *regex, char const *by)
{
    if (!sbl_split_string_in_nline(sb) ||
        !sbl_replace(sb, regex, by))
            return 0 ;

    return sbl_rebuild_nline(sb) ;
}
