/*
 * socket_private.c
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

#include <sys/socket.h>
#include <fcntl.h>

#include <oblibs/socket.h>

int socket_private(int domain, int type, int protocol, unsigned int flags)
{
    int f = type | ((flags & O_NONBLOCK) ? SOCK_NONBLOCK : 0) | ((flags & O_CLOEXEC) ? SOCK_CLOEXEC : 0) ;
    return socket(domain, f, protocol) ;
}