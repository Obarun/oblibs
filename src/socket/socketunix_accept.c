/*
 * socketunix_accept.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */


#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <oblibs/socket.h>

int socketunix_accept(int s, unsigned int flags)
{
    struct sockaddr_un sa ;
    socklen_t dummy = sizeof sa ;
    int fd ;
    unsigned int opt = ((flags & O_NONBLOCK) ? SOCK_NONBLOCK : 0) | ((flags & O_CLOEXEC) ? SOCK_CLOEXEC : 0) ;
    do {
        fd = accept4(s, (struct sockaddr *)&sa, &dummy, opt) ;
    } while ((fd < 0) && (errno == EINTR)) ;

    if (fd < 0)
        return -1 ;

    return fd ;
}
