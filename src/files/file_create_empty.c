/*
 * file_create_empty.c
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

#include <oblibs/files.h>

#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#include <skalibs/djbunix.h>

int file_create_empty (char const *dst, char const *name,mode_t mode)
{
    int fd, end = 0 ;
    size_t dstlen = strlen(dst) ;
    size_t namelen = strlen(name) ;
    char file[dstlen + 1 + namelen + 1] ;
    if (dst[0] != '/') return 0 ;
    if (dst[dstlen-1] != '/') end++ ;
    memcpy(file,dst,dstlen) ;
    if (end)
        file[dstlen] = '/' ;
    memcpy(file + dstlen + end,name,namelen) ;
    file[dstlen + end + namelen] = 0 ;
    fd = open3 (file, O_WRONLY | O_NONBLOCK | O_CREAT | O_EXCL, mode) ;
    if (fd == -1){
        fd_close(fd) ;
        return 0;
    }
    fd_close(fd) ;
    return 1 ;
}
