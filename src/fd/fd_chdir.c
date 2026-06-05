/*
 * fd_chdir.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/** This function comes from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/fd_chdir.c
 * under license ISC. All credits go to Laurent Bercot, author of this function. */

#include <oblibs/fd.h>

#include <errno.h>
#include <unistd.h>

int fd_chdir(int fd)
{
    int e = errno ;
    int r ;
    do r = fchdir(fd) ;
    while (r == -1 && errno == EINTR) ;
    if (r >= 0) errno = e ;
    return r ;
}
