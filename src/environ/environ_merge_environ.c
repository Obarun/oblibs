/*
 * environ_merge_environ.c
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

#include <errno.h>
#include <string.h>

#include <oblibs/sastr.h>
#include <oblibs/lexer.h>
#include <oblibs/stack.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>

uint8_t environ_merge_environ(stralloc *modifs, stralloc *new)
{
    size_t pos = 0 ;
    _alloc_sa_(sa) ;

    if (!modifs->len)
        return 1 ;

    if (!stralloc_catb(&sa, new->s, new->len))
        return (errno = EOVERFLOW, 0) ;

    if (!environ_rebuild(new))
        return 0 ;

    lexer_config c = LEXER_CONFIG_ZERO ;
    _alloc_stk_(nkey, new->len + 1) ;

    /**
     * Search from @modifs environment if a
     * same key=value pair exist comparing their
     * respective keys name.
     */
    FOREACH_SASTR(modifs, pos) {

        char *line = modifs->s + pos ;
        _alloc_stk_(ekey, strlen(line) + 1) ;
        lexer_reset_hard(&c) ;

        // get the @modifs environment key
        if (!environ_get_key(&ekey, line))
            return 0 ;

        // search for the @modifs key to the new environment
        if (!environ_search_key(&nkey, new->s, ekey.s, &c)) {
            if (errno == EOVERFLOW)
                return (errno = EOVERFLOW, 0) ;
            if (!sastr_add_string(&sa, line))
                return 0 ;
        }
    }

    // flush the modifs environment with the new one
    if (!stralloc_copyb(modifs, sa.s, sa.len) ||
        !stralloc_0(modifs))
            return (errno = EOVERFLOW, 0) ;

    modifs->len-- ;

    return 1 ;
}
