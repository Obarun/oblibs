/*
 * environ_substitute.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
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
#include <errno.h>
#include <stdlib.h> // getenv

#include <oblibs/environ.h>
#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>
#include <oblibs/genbuf.h>
#include <oblibs/subst.h>

#include "environ-internal.h"

static int dollar_find(strbuf *regex, const char *line)
{
    lexer_config cfg = LEXER_CONFIG_ZERO ;

    cfg.open = "${" ; cfg.olen = 2 ; cfg.close = "}" ;  cfg.clen = 1 ;
    cfg.forceclose = 0 ; cfg.firstoccurence = 1 ;
    cfg.style = 1 ; cfg.kopen = 0 ; cfg.kclose = 0 ;

    cfg.str = line ; cfg.slen = strlen(line) ;

    while(cfg.pos < cfg.slen) {

        lexer_reset(&cfg) ;
        regex->len = 0 ;
        cfg.opos = cfg.cpos = 0 ;

        if (!lexer(regex, &cfg) ||
            !strbuf_uncounted(regex))
                return 0 ;

        if (cfg.found)
            return 1 ;
     }

     return 0 ;
}

/* @varsback/@valuesback are the LENGTHS info->vars/info->values had when the
 * current entry was entered -- the rollback point. They are deliberately NOT the
 * entry's var/value offsets: since a repeated name reuses the offset of a name
 * recorded earlier, truncating to that offset would destroy already registered
 * names. */
static int dollar_recursive(strbuf *value, const char *key, const char *old, size_t olen, subst_t *info, size_t varsback, size_t valuesback)
{
    size_t klen = strlen(key) ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _alloc_strbuf_(nvalue, olen + 1) ;
    _alloc_strbuf_(nkey, klen + 4) ;

    if (!environ_sbl_value(&nvalue, old, olen, key))
        /** don't die here. If it doesn't exist, leave it as it*/
        return 2 ;

    if (!strbuf_copyb(&sa, value->s, strlen(value->s)) ||
        !strbuf_uncounted(&sa))
        return (errno = EOVERFLOW, 0) ;

    if (!auto_strbuf(&nkey, "${", key, "}"))
        return (errno = EOVERFLOW, 0) ;
    uint8_t unexport = (nvalue.s[0] == VAR_UNEXPORT) ? 1 : 0 ;

    if (!sbl_replace(&sa, nkey.s, nvalue.s + unexport)) {
        info->vars.len = varsback ;
        info->values.len = valuesback ;
        return 0 ;
    }

    /* copy sa.s WITH its NUL so value stays a valid C string (value.s is read
     * by strlen/sbl_replace later); value.len is never used as a length. */
    if (!strbuf_copyb(value, sa.s, strlen(sa.s) + 1))
        return (errno = EOVERFLOW, 0) ;

    return 1 ;
}

/* @varsback/@valuesback: see dollar_recursive. */
static int dollar_replace(strbuf *old, const char *line, const char *mkey, subst_t *info, size_t varsback, size_t valuesback)
{

    int r ;
    _alloc_strbuf_(new, old->len + 1) ;
    _alloc_strbuf_(regex, old->len + 1) ;
    _alloc_strbuf_(value, old->len + 1) ;
    lexer_config cfg = LEXER_CONFIG_ZERO ;

    if (!strbuf_copyb(&new, old->s, old->len) ||
        !strbuf_uncounted(&new))
           return (errno = EOVERFLOW, 0) ;

    cfg.open = "${" ; cfg.olen = 2 ; cfg.close = "}" ;  cfg.clen = 1 ;
    cfg.forceclose = 0 ; cfg.firstoccurence = 1 ;
    cfg.style = 1 ; cfg.kopen = 0 ; cfg.kclose = 0 ;

    cfg.str = line ; cfg.slen = strlen(line) ;

    while(cfg.pos < cfg.slen) {

        lexer_reset(&cfg) ;
        regex.len = 0 ;
        cfg.opos = cfg.cpos = 0 ;

        if (!lexer(&regex, &cfg) ||
            !strbuf_uncounted(&regex))
                return 0 ;

        if (cfg.found) {

            /** Search first in the current environment */
            char *gval = getenv(regex.s) ;
            if (!gval) {

                if (!strcmp(mkey, regex.s)) {

                    log_warn("recursive call of variable: ", regex.s, " -- ignoring it") ;
                    continue ;
                }

                /** don't die here. If it doesn't exist, leave it as it*/
                if (!environ_sbl_value(&value, old->s, old->len, regex.s))
                    continue ;

            } else {
                /* copy WITH the NUL: value.s must stay a valid C string */
                if (!strbuf_copyb(&value, gval, strlen(gval) + 1))
                    return (errno = EOVERFLOW, 0) ;
            }

            {
                /** value may also contain ${whatever} variable.
                 * try to find it out before appending the value
                 * to exlsn_t *info struct */

                _alloc_strbuf_(nregex, value.len + 1) ;
                /* Keys already expanded in this chain. A key reappearing means a
                 * cyclic reference (e.g. A=${B}, B=${A}), which would otherwise
                 * loop forever -- value just oscillates between ${A} and ${B}.
                 * The same-name guard above only sees one step, so indirect
                 * cycles must be caught here. */
                _alloc_sbl_(seen, value.len + 1) ;

                while (dollar_find(&nregex, value.s)) {

                    if (sbl_search(&seen, nregex.s) >= 0) {
                        log_warn("cyclic reference of variable: ", nregex.s, " -- ignoring it") ;
                        break ;
                    }
                    if (!sbl_add(&seen, nregex.s))
                        return (errno = EOVERFLOW, 0) ;

                    r = dollar_recursive(&value, nregex.s, old->s, old->len, info, varsback, valuesback) ;
                    if (!r)
                      return 0 ;

                    if (r == 2)
                      goto next ;
                }
            }

            next:
            if (regex.len) {

                _alloc_strbuf_(key, regex.len + 4) ;
                if (!auto_strbuf(&key, "${", regex.s, "}"))
                    return (errno = EOVERFLOW, 0) ;
                uint8_t unexport = (value.s[0] == VAR_UNEXPORT) ? 1 : 0 ;

                if (!sbl_replace(&new, key.s, value.s + unexport)) {
                    info->vars.len = varsback ;
                    info->values.len = valuesback ;
                    return 0 ;
                }

            }
        }

        old->len = 0 ;

        if (!strbuf_copyb(old, new.s, new.len) ||
            !strbuf_uncounted(old))
                return (errno = EOVERFLOW, 0) ;
    }

    return 1 ;
}

uint8_t environ_substitute(strbuf *modifs, subst_t *info)
{
    if (!modifs->len)
        return 1 ;

    size_t pos = 0 ;
    uint8_t unexport = 0 ;
    subst_entry_t blah ;
    _alloc_strbuf_(new, modifs->len + 1) ;

    if (!strbuf_copyb(&new, modifs->s, modifs->len) ||
        !strbuf_uncounted(&new))
            return (errno = EOVERFLOW, 0) ;

    FOREACH_SBL(modifs, pos) {

        char *line = modifs->s + pos ;
        size_t slen = strlen(line) ;
        unexport = 0 ;

        _alloc_strbuf_(mkey, slen + 1) ;
        _alloc_strbuf_(mval, slen + 1) ;

        if (!environ_get_key(&mkey, line))
            return 0 ;

        if (!environ_get_value(&mval, line))
            return 0 ;

        if (mval.s[0] == VAR_UNEXPORT)
            unexport = 1 ;

        /* Rollback point of THIS entry: the blob lengths on arrival. Kept apart
         * from blah.var/blah.value, which are the entry's offsets and may point
         * back into what a previous entry (or a previous call) registered. */
        size_t varsback = info->vars.len ;
        size_t valuesback = info->values.len ;

        /* One name at most in info->vars: reuse the offset when it is already
         * recorded, append it otherwise. */
        ssize_t namepos = sbl_search(&info->vars, mkey.s) ;

        if (namepos < 0) {

            blah.var = info->vars.len ;

            if (!strbuf_catb(&info->vars, mkey.s, mkey.len + 1)) {
                info->vars.len = varsback ;
                info->values.len = valuesback ;
                return 0 ;
            }

        } else {

            blah.var = (size_t)namepos ;
        }

        /* One occurrence at most in info->modifs, last source wins: the marker
         * adds the key, its absence removes it. A caller reads that list as bare
         * KEY records that UNSET the variable, so a key re-exported by a later
         * source has to leave it. */
        if (unexport) {

            if (sbl_search(&info->modifs, mkey.s) < 0) {

                if (!strbuf_catb(&info->modifs, mkey.s, mkey.len + 1)) {
                    info->vars.len = varsback ;
                    info->values.len = valuesback ;
                    return (errno = EOVERFLOW, 0) ;
                }
            }

        } else {

            /* cannot fail: a no-op when absent, and sbl_remove_at only rejects
             * an offset sbl_search did not return */
            sbl_remove(&info->modifs, mkey.s) ;
        }

        if (!dollar_replace(&new, line, mkey.s, info, varsback, valuesback))
            return 0 ;

        _alloc_strbuf_(nval, new.len + 1) ;
        if (!environ_sbl_value(&nval, new.s, new.len, mkey.s))
            return 0 ;

        nval.s[strlen(nval.s)] = 0 ;

        /* The new value is always appended at the END of info->values, so the
         * offsets already handed out stay valid. Overwriting an entry therefore
         * leaves its former value unreferenced in the blob; those few dead bytes
         * are accepted, the context is short-lived and reindexing would cost far
         * more. */
        blah.value = info->values.len ;

        if (!nval.s[0]) {

            blah.n = 0 ;

        } else {

            unexport = (nval.s[0] == VAR_UNEXPORT) ? 1 : 0 ;
            if (!strbuf_cats(&info->values, nval.s + unexport)) {
                info->vars.len = varsback ;
                info->values.len = valuesback ;
                return (errno = EOVERFLOW, 0) ;
            }

            /* el_transform with a zero eltransforminfo only NUL-terminates the
             * value and yields a single value (n=1); reproduce that directly. */
            if (!strbuf_terminate(&info->values)) {
                info->vars.len = varsback ;
                info->values.len = valuesback ;
                return (errno = EOVERFLOW, 0) ;
            }

            blah.n = 1 ;
        }

        /* Last, once every fallible append succeeded: one entry at most per name,
         * so a name already carrying an entry has it REWRITTEN in place. Done
         * here, there is never a half-written entry to restore. Names being
         * unique in info->vars, blah.var identifies the entry exactly. */
        {
            subst_entry_t *d = genbuf_s(subst_entry_t, &info->data) ;
            size_t n = genbuf_len(subst_entry_t, &info->data), i = 0 ;

            for (; i < n ; i++)
                if (d[i].var == blah.var)
                    break ;

            if (i < n) {

                d[i] = blah ;

            } else if (!genbuf_append(subst_entry_t, &info->data, &blah)) {

                info->vars.len = varsback ;
                info->values.len = valuesback ;
                return 0 ;
            }
        }
    }

    if (!strbuf_copyb(modifs, new.s, new.len) ||
        !strbuf_uncounted(modifs))
            return (errno = EOVERFLOW, 0) ;

    return 1 ;
}
