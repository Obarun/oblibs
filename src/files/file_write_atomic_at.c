/*
 * file_write_atomic_at.c
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

#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

int file_write_atomic_at(char const *tmpdir, char const *file, char const *content, size_t len)
{
    size_t dlen = strlen(tmpdir) ;

    if (!dlen) {
        errno = EINVAL ;
        return 0 ;
    }

    char const *base = strrchr(file, '/') ;
    base = base ? base + 1 : file ;

    size_t sep = tmpdir[dlen - 1] == '/' ? 0 : 1 ;
    char tmp[dlen + sep + strlen(base) + 7 + 1] ;
    auto_strings(tmp, tmpdir, sep ? "/" : "", base, ".XXXXXX") ;

    int fd = file_tmp_open(tmp, 0644, (uid_t)-1, (gid_t)-1) ;
    if (fd < 0)
        return 0 ;

    if (io_allwrite(fd, (char *)content, len) < len) {
        close_fd(fd) ;
        file_tryunlink(tmp) ;
        return 0 ;
    }

    return file_tmp_rename_at(fd, tmp, file) ;
}
