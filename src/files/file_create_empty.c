/*
 * file_create_empty.c
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <oblibs/io.h>

int file_create_empty (char const *dst, char const *name,mode_t mode)
{
    int end = 0 ;
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

    int e = errno ;
    int fd = io_open_mode(file, O_WRONLY | O_NONBLOCK | O_CREAT | O_EXCL, mode) ;
    if (fd < 0) return 0 ;       /* errno = open error */
    close(fd) ;
    errno = e ;                  /* restore entry errno on success */
    return 1 ;
}
