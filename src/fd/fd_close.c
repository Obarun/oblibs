/*
 * fd_close.c
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
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/fd_close.c
 * under license ISC. All credits go to Laurent Bercot, author of this function. */

#include <errno.h>
#include <unistd.h>

void fd_close(int fd)
{
    int e = errno ;
    close(fd) ;
    errno = e ;
}
