/*
 * file_openclose_readonly.c
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <oblibs/files.h>
#include <oblibs/io.h>

int file_openclose_readonly(char *buffile, char const *filename, size_t buffsize)
{
    int e = errno ;

    int fd = io_open(filename, O_RDONLY) ;
    if (fd < 0) return -1 ;

    size_t got = io_allread(fd, buffile, buffsize) ;
    int re = errno ;
    close(fd) ;

    if (got < buffsize && re != EPIPE)   /* short read not caused by EOF = real error */
        return (errno = re, -1) ;

    errno = e ;
    return fd ;
}
