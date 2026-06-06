/*
 * opt_fields.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * Layer 3: parsing of structured option VALUES ("k=v:k=v,v"). Built ABOVE the
 * lexer (strbuf <- lexer <- opt_fields), so the dependency stays acyclic.
 */

#include <string.h>

#include <oblibs/opt.h>
#include <oblibs/lexer.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/log.h>

uint8_t opt_list(strbuf *out, char const *val, char sep)
{
    if (!val || !*val)
        return 1 ;
    return lexer_trim_with_delim(out, val, (unsigned int)sep) ;
}

uint8_t opt_fields(char const *str, char fieldsep, char kvsep, opt_field_t const *fields, size_t n, opt_field_fn *fn, void *data)
{
    if (!str || !*str)
        return 1 ;

    size_t pos = 0 ;
    _alloc_strbuf_(sb, strlen(str) + 1) ;

    /* split on fieldsep (e.g. ':') into a NUL-separated list */
    if (!lexer_trim_with_delim(&sb, str, (unsigned int)fieldsep))
        log_warn_return(0, "parse options: ", str) ;

    FOREACH_SBL(&sb, pos) {

        char const *e = sb.s + pos ;
        size_t elen = strlen(e) ;
        char tmp[elen + 1] ;
        memcpy(tmp, e, elen + 1) ;

        /* split the element on kvsep (e.g. '=') into key / val */
        char *eq = strchr(tmp, kvsep) ;
        char const *key = tmp, *val = 0 ;
        if (eq) {
            *eq = 0 ;
            val = eq + 1 ;
        }

        size_t i = 0 ;
        for (; i < n ; i++)
            if (!strcmp(fields[i].key, key))
                break ;

        if (i >= n)
            log_warn_return(0, "invalid option key: ", key) ;

        if (fields[i].hasval && !val)
            log_warn_return(0, "option requires a value: ", key) ;

        if (!fields[i].hasval && val)
            log_warn_return(0, "option takes no value: ", key) ;

        if (!fn(fields[i].id, key, val, data))
            return 0 ;
    }

    return 1 ;
}
