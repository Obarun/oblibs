/*
 * copy_fd.c
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

#include <unistd.h>
#include <errno.h>

#include <oblibs/fd.h>

int copy_fd(int to, int from)
{
    int e = errno ;
    int r ;

    if (to == from)
        return (errno = EINVAL, -1) ;

    do r = dup2(from, to) ;
    while (r == -1 && errno == EINTR) ;

    if (r >= 0)
        errno = e ;   /* preserve errno on success */

    return r ;
}
