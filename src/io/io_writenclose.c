/*
 * io_writenclose.c
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

#include <sys/types.h>
#include <stddef.h>
#include <errno.h>

#include <oblibs/io.h>
#include <oblibs/fd.h>

int io_writenclose(int fd, char const *s, size_t len)
{
    if (fd < 0)
        return 0 ;

    int r = io_allwrite(fd, (char *)s, len) == len ;
    close_fd(fd) ; /* close_fd preserves errno (set by io_allwrite on failure) */

    return r ;
}
