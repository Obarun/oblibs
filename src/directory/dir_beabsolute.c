/*
 * dir_beabsolute.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>

#include <oblibs/directory.h>
#include <oblibs/string.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>

int dir_beabsolute(char *dst,char const *dir)
{
    uint8_t file = 0 ;
    size_t dirlen = 4096 ;
    int e = errno ;
    int fd = io_open(".", O_RDONLY | O_NONBLOCK) ;
    if (fd < 0) return 0 ;
    if (chdir(dir) < 0) {

        if (errno == ENOTDIR) {

            if (chdir(".") < 0)
                goto err ;
            file = 1 ;
        }
        else
            goto err ;
    }
    char *p = getcwd(dst,dirlen) ;
    if (!p) goto err ;
    if (fd_chdir(fd) < 0) goto err ;
    fd_close(fd) ;
    errno = e ;
    auto_strings(dst, p, "/", file ? dir : 0) ;
    return 1 ;
    err:
        fd_close(fd) ;
        errno = e ;
        return 0 ;
}
