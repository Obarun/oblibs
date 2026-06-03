/*
 * file_write_unsafe_g.c
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
#include <fcntl.h>
#include <unistd.h>

#include <oblibs/io.h>

int file_write_unsafe_g(char const *file, char const *content)
{
    size_t contlen = strlen(content) ;

    int fd = io_open_mode(file, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT | O_CLOEXEC, 0666) ;
    if (fd < 0) return 0 ;

    if (io_allwrite(fd, (char *)content, contlen) != contlen) {
        int e = errno ;
        close(fd) ;
        unlink(file) ;
        return (errno = e, 0) ;
    }

    close(fd) ;
    return 1 ;
}
