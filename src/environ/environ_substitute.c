/*
 * environ_substitute.c
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
#include <stdlib.h> // getenv

#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <execline/execline.h>

uint8_t environ_substitute(stralloc *modifs, exlsn_t *info)
{
    if (!modifs->len)
        return 1 ;
    size_t pos = 0 ;
    uint8_t unexport = 0 ;
    eltransforminfo_t si = ELTRANSFORMINFO_ZERO ;
    elsubst_t blah ;
    _alloc_sa_(old) ; _alloc_sa_(new) ;
    _alloc_stk_(regex, modifs->len + 1) ;

    lexer_config cfg = LEXER_CONFIG_ZERO ;

    cfg.open = "${" ; cfg.olen = 2 ; cfg.close = "}" ;  cfg.clen = 1 ;
    cfg.forceclose = 0 ; cfg.firstoccurence = 1 ;
    cfg.style = 1 ; cfg.kopen = 0 ; cfg.kclose = 0 ;

    if (!stralloc_copyb(&old, modifs->s, modifs->len) ||
        !stralloc_copyb(&new, modifs->s, modifs->len) ||
        !stralloc_0(&old) ||
        !stralloc_0(&new))
            return (errno = EOVERFLOW, 0) ;

    old.len-- ; new.len-- ;

    if (!environ_rebuild(&old))
        return 0 ;

    FOREACH_SASTR(modifs, pos) {

        lexer_reset_hard(&cfg) ;
        stack_reset(&regex) ;
        char *line = modifs->s + pos ;
        cfg.str = line ; cfg.slen = strlen(line) ;
        unexport = 0 ;

        _alloc_stk_(mkey, cfg.slen + 1) ;
        _alloc_stk_(mval, cfg.slen + 1) ;
        _alloc_stk_(value, old.len + 1) ;

        if (!environ_get_key(&mkey, line))
            return 0 ;

        if (!environ_get_value(&mval, line))
            return 0 ;

        if (mval.s[0] == VAR_UNEXPORT)
            unexport = 1 ;

        blah.var = info->vars.len ;
        blah.value = info->values.len ;

        if (sastr_cmp(&info->vars, mkey.s) == -1) {

            if (!stralloc_catb(&info->vars, mkey.s, mkey.len + 1)) {
                info->vars.len = blah.var ;
                info->values.len = blah.value ;
                return 0 ;
            }
        }

        if (unexport) {

            if (!stralloc_catb(&info->modifs, mkey.s, mkey.len + 1)) {
                info->vars.len = blah.var ;
                info->values.len = blah.value ;
                return (errno = EOVERFLOW, 0) ;
            }
        }

        while(cfg.pos < cfg.slen) {

            lexer_reset(&cfg) ;
            stack_reset(&regex) ;
            cfg.opos = cfg.cpos = 0 ;

            if (!lexer(&regex, &cfg) ||
                !stack_close(&regex))
                    return 0 ;

            if (cfg.found) {

                stack_reset(&value) ;

                /** Search first in the current environment */
                char *gval = getenv(regex.s) ;
                if (!gval) {

                    if (!strcmp(mkey.s, regex.s)) {
                        log_warn("recursive call of variable: ", regex.s, " -- ignoring it") ;
                        continue ;
                    }

                    /** don't die here. If it doesn't exist, leave it as it*/
                    if (!environ_search_value(&value, old.s, regex.s))
                        continue ;

                } else {
                    auto_strings(value.s, gval) ;
                }

                if (regex.len) {

                    _alloc_stk_(key, regex.len + 4) ;
                    auto_strings(key.s, "${", regex.s, "}") ;

                    if (!sastr_replace(&new, key.s, value.s)) {
                        info->vars.len = blah.var ;
                        info->values.len = blah.value ;
                        return 0 ;
                    }
                }
            }

            old.len = 0 ;

            if (!stralloc_copyb(&old, new.s, new.len) ||
                !stralloc_0(&old))
                    return (errno = EOVERFLOW, 0) ;

            old.len-- ;

            if (!environ_rebuild(&old))
                return 0 ;
        }

        _alloc_stk_(nval, old.len + 1) ;
        if (!environ_search_value(&nval, old.s, mkey.s))
            return 0 ;

        if (!nval.s[0]) {
            blah.n = 0 ;
        } else {

            int r ;
            unexport = (nval.s[0] == VAR_UNEXPORT) ? 1 : 0 ;
            if (!stralloc_cats(&info->values, nval.s + unexport)) {
                info->vars.len = blah.var ;
                info->values.len = blah.value ;
                return (errno = EOVERFLOW, 0) ;
            }
            r = el_transform(&info->values, blah.value, &si) ;
            if (r < 0) {
                info->vars.len = blah.var ;
                info->values.len = blah.value ;
                return 0 ;
            }

            blah.n = r ;
        }

        if (!genalloc_append(elsubst_t, &info->data, &blah)) {
            info->vars.len = blah.var ;
            info->values.len = blah.value ;
            return 0 ;
        }
    }

    if (!stralloc_copyb(modifs, new.s, new.len) ||
        !stralloc_0(modifs))
            return (errno = EOVERFLOW, 0) ;
    modifs->len-- ;

    return 1 ;
}
