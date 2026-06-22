/*
 * file_copy.c
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
 *
 * Native replacement for skalibs filecopy_unsafe (djbunix). Direct, non-atomic
 * copy of a regular file's contents: dst is created/truncated up front, so a
 * mid-copy failure leaves a partial dst. Credit: skalibs (ISC, Laurent Bercot).
 */

#include <fcntl.h>

#include <oblibs/files.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

int file_copy(char const *src, char const *dst, mode_t mode)
{
    int s = io_open(src, O_RDONLY) ;
    if (s < 0)
        return 0 ;

    int d = io_open_mode(dst, O_WRONLY | O_CREAT | O_TRUNC, mode) ;
    if (d < 0) {
        close_fd(s) ;
        return 0 ;
    }

    char buf[65536] ;
    for (;;) {
        ssize_t r = io_read(s, buf, sizeof buf) ;
        if (r < 0)
            goto err ;
        if (!r)
            break ;
        if (io_allwrite(d, buf, (size_t)r) < (size_t)r)
            goto err ;
    }

    close_fd(d) ;
    close_fd(s) ;
    return 1 ;

 err:
    close_fd(d) ;
    close_fd(s) ;
    return 0 ;
}
