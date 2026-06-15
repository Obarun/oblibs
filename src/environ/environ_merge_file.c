/*
 * environ_merge_file.c
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
#include <stddef.h>
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/files.h>

uint8_t environ_merge_file(strbuf *modifs, const char *file)
{
    size_t filesize = file_get_size(file) ;
    if (!filesize)
        return (errno = EINVAL, 0) ;
    _alloc_strbuf_(stk, filesize + 1) ;

    if (!environ_parse_file(&stk, file))
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
