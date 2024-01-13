/*
 * dir_beabsolute.c
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

#include <oblibs/directory.h>

#include <unistd.h>
#include <string.h>

#include <skalibs/djbunix.h>

int dir_beabsolute(char *dst,char const *dir)
{
    size_t dirlen = 4096 ;
    int fd = open_read(".") ;
    if (fd < 0) return 0 ;
    if (chdir(dir) < 0) goto err ;
    char *p = getcwd(dst,dirlen) ;
    if (!p) goto err ;
    if (fd_chdir(fd) < 0) goto err ;
    fd_close(fd) ;
    return 1 ;
    err:
        fd_close(fd) ;
        return 0 ;
}
