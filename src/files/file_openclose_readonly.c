/*
 * file_openclose_readonly.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <skalibs/djbunix.h>

int file_openclose_readonly(char *buffile, char const *filename, size_t buffsize)
{
    int fd, res, e ;
    e = errno ;
    errno = 0 ;

    fd = open(filename, O_RDONLY) ;

    if (errno)
    {
        return -1 ;
    }
    res = read(fd,buffile,buffsize) ;
    if (res < 0){ fd_close (fd) ; return -1 ; }
    fd_close(fd) ;
    errno = e ;
    return fd;
}
