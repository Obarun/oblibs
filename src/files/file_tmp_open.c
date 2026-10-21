/*
 * file_tmp_open.c
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/files.h>
#include <oblibs/fd.h>

int file_tmp_open(char *tmpl, mode_t mode, uid_t uid, gid_t gid)
{
    int fd = mkstemp(tmpl) ;
    if (fd < 0)
        return -1 ;

    if (!cloexec_fd(fd))
        goto err ;

    // chown first: it clears S_ISUID/S_ISGID that fchmod may set
    if ((uid != (uid_t)-1 || gid != (gid_t)-1) && fchown(fd, uid, gid) < 0)
        goto err ;

    if (fchmod(fd, mode) < 0)
        goto err ;

    return fd ;

    err:
        close_fd(fd) ;
        file_tryunlink(tmpl) ;
        return -1 ;
}
