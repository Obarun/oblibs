/*
 * socket_accept.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */


#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <oblibs/log.h>
#include <oblibs/socket.h>

int socket_accept(int socket)
{
    int fd ;

    do {
        fd = accept(socket, NULL, NULL) ;
    } while ((fd < 0) && (errno == EINTR)) ;

    if (fd < 0) {
        log_warnusys("accept socket connection") ;
        return -1 ;
    }

    if (!socket_set_coe(fd) || !socket_set_nonblock(fd)) {
        log_warnusys("set O_NONBLOCK | FD_CLOEXEC on socket") ;
        return -1 ;
    }

    return fd ;
}
