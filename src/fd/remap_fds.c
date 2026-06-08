/*
 * remap_fds.c
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
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/fd_move2.c
 * under license ISC. All credits go to Laurent Bercot, author of this function.
 * adapted for oblibs by Eric Vidal <eric@obarun.org>.*/

#include <errno.h>
#include <unistd.h>

#include <oblibs/fd.h>

int remap_fds(int to1, int from1, int to2, int from2)
{
    int tmp = from2 ;

    if (to1 == from1) return move_fd(to2, from2) ;
    if (to2 == from2) return move_fd(to1, from1) ;
    if (from1 == from2) return (to1 == to2) ? move_fd(to1, from1) : (errno = EINVAL, -1) ;
    if (to1 == to2) return (errno = EINVAL, -1) ;

    if (from2 == to1) {              /* moving onto to1 would clobber from2: park it */
        tmp = dup(from2) ;
        if (tmp == -1) return -1 ;
    }

    if (copy_fd(to1, from1) == -1) {
        if (from2 != tmp) close_fd(tmp) ;
        return -1 ;
    }

    if (copy_fd(to2, tmp) == -1) {
        int e = errno ;
        close_fd(to1) ;
        if (from2 != tmp) move_fd(from2, tmp) ;   /* roll the parked fd back */
        errno = e ;
        return -1 ;
    }

    if (from1 != to2) close_fd(from1) ;
    close_fd(tmp) ;
    return 0 ;
}
