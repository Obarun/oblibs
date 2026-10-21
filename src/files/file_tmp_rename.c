/*
 * file_tmp_rename.c
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

#include <stdio.h>
#include <unistd.h>

#include <oblibs/files.h>

int file_tmp_rename(int fd, char const *tmp, char const *dst)
{
    // no retry on EINTR: Linux releases the descriptor even when close fails
    if (close(fd) < 0 || rename(tmp, dst) < 0) {
        file_tryunlink(tmp) ;
        return 0 ;
    }

    return 1 ;
}
