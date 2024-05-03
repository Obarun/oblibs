/*
 * sastr_replace.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <skalibs/stralloc.h>

int sastr_replace (stralloc *sa,char const *regex,char const *by)
{
    if (!sa->len || !regex || !strcmp(regex,by))
        return (errno = EINVAL, 0) ;

    size_t pos = 0, bylen = strlen(by) ;
    lexer_config cfg = LEXER_CONFIG_ZERO ;
    _init_stack_(stk, sa->len) ;
    _alloc_sa_(new) ;

    cfg.open = (char *)regex ; cfg.olen = strlen(regex) ;
    cfg.close = "" ;  cfg.clen = 1 ; cfg.style = 1 ; cfg.kopen = 0 ; cfg.kclose = 0 ;
    cfg.forceclose = 1 ; cfg.firstoccurence = 1 ;

    FOREACH_SASTR(sa, pos) {

        stack_reset(&stk) ;
        lexer_reset_hard(&cfg) ;
        cfg.str = sa->s + pos ; cfg.slen = strlen(sa->s + pos) ;

        if (!lexer(&stk, &cfg))
            return 0 ;

        if (cfg.found) {

            _init_stack_(line, cfg.slen + bylen + 1) ;

            if (!stack_add(&line, cfg.str, cfg.opos) ||
                !stack_add(&line, by, bylen) ||
                !stack_close(&line))
                    return 0 ;

            if (stk.len) {
                /** the rest of the line may contain @regex*/
                _init_stack_(c, new.len + 1) ;
                if (!stack_copy(&c, new.s, new.len))
                    return 0 ;

                new.len = 0 ;

                if (!stralloc_copyb(&new, stk.s, stk.len) ||
                    !stralloc_0(&new) ||
                    !sastr_replace(&new, regex, by) ||
                    !stralloc_insertb(&new, 0, line.s, line.len) ||
                    !stralloc_insertb(&new, 0, c.s, c.len))
                        return 0 ;

            } else if (!sastr_add_string(&new, line.s))
                return 0 ;

        } else if (!sastr_add_string(&new, cfg.str))
            return 0 ;
    }

    if (!stralloc_copyb(sa, new.s, new.len) ||
        !stralloc_0(sa))
            return 0 ;

    sa->len-- ;
    return 1 ;
}
