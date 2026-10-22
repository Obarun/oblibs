/*
 * dir_replace_at.c
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

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

int dir_replace_at(char const *tmpdir, char const *src, char const *dst)
{
    int e ;
    char name[strlen(dst) + 1] ;
    _cleanup_strbuf_ strbuf old = STRBUF_ZERO ;

    if (!ob_basename(name, dst))
        return 0 ;

    if (!dir_create_tmp(&old, tmpdir, name))
        return 0 ;

    if (rename(dst, old.s) < 0) {
        e = errno ;
        dir_destroy(old.s) ;
        errno = e ;
        return e == ENOENT ? file_tmp_rename(src, dst) : 0 ;
    }

    if (!file_tmp_rename(src, dst)) {
        e = errno ;
        rename(old.s, dst) ;
        errno = e ;
        return 0 ;
    }

    dir_destroy(old.s) ;

    return 1 ;
}
