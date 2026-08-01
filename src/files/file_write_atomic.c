/*
 * file_write_atomic.c
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
 */

#include <stddef.h>
#include <string.h>

#include <oblibs/files.h>

int file_write_atomic(char const *file, char const *content, size_t len)
{
    char const *slash = strrchr(file, '/') ;
    // dirname of @file: "." with no slash, "/" for a root-level file
    size_t dlen = !slash || slash == file ? 1 : (size_t)(slash - file) ;
    char tmpdir[dlen + 1] ;

    if (!slash) {
        tmpdir[0] = '.' ;
    } else {
        memcpy(tmpdir, file, dlen) ; // slash == file copies the leading '/'
    }

    tmpdir[dlen] = 0 ;

    return file_write_atomic_at(tmpdir, file, content, len) ;
}
