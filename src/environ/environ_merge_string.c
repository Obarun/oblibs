/*
 * environ_merge_string.c
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
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

#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/log.h>

uint8_t environ_merge_string(strbuf *modifs, const char *str)
{
    if (!str)
        return (errno = EINVAL, 0) ;
    size_t slen = strlen(str) ;
    if (slen >= MAXENV) {
        log_warn("too many bytes in string") ;
        return (errno = ENAMETOOLONG, 0) ;
    }

    _alloc_strbuf_(stk, strlen(str) + 1) ;

    if (!environ_trim(&stk, str))
        return 0 ;

    if (!modifs->len) {
        /** no previous file, copy verbatim*/
        if (!strbuf_copyb(modifs, stk.s, stk.len) ||
            !strbuf_terminate(modifs))
                return (errno = EOVERFLOW, 0) ;

        modifs->len-- ;
        return 1 ;
    }

    _alloc_strbuf_(new, stk.len + 1) ;

    if (!strbuf_copyb(&new, stk.s, stk.len) ||
        !strbuf_terminate(&new))
            return (errno = EOVERFLOW, 0) ;
    new.len-- ;
    if (!environ_merge_environ(modifs, &new))
        return 0 ;

    return 1 ;

}
