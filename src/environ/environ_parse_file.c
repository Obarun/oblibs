/*
 * environ_parse_file.c
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
#include <errno.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

uint8_t environ_parse_file(strbuf *sb, const char *file)
{
    ssize_t flen = file_get_size(file) ;
    if (flen <= 0 || flen >= MAXENV) {
        if (!flen)
            errno = EINVAL ;
        else if (flen >= MAXENV) {
            log_warn("too many bytes in file: ", file) ;
            errno = ENAMETOOLONG ;
        }
        return 0 ;
    }

    _alloc_strbuf_(f, flen + 1) ;
    if (!strbuf_read_file(&f, file))
        return 0 ;

    if (!environ_trim(sb, f.s))
        return 0 ;

    return 1 ;
}

