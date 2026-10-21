/*
 * file_tmp_open_at.c
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
#include <errno.h>
#include <sys/types.h>

#include <oblibs/files.h>
#include <oblibs/string.h>

int file_tmp_open_at(char *tmp, size_t tmplen, char const *dir, mode_t mode, uid_t uid, gid_t gid)
{
    size_t dlen = strlen(dir) ;

    if (!dlen)
        return (errno = EINVAL, -1) ;

    size_t sep = dir[dlen - 1] == '/' ? 0 : 1 ;

    if (dlen + sep + 6 + 1 > tmplen)
        return (errno = ENAMETOOLONG, -1) ;

    auto_strings(tmp, dir, sep ? "/" : "", "XXXXXX") ;

    return file_tmp_open(tmp, mode, uid, gid) ;
}
