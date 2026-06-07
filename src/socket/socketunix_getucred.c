/*
 * socketunix_getucred.c
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

#include <oblibs/socket.h>

int socketunix_getucred(int fd, struct ucred *cred)
{
    socklen_t len = sizeof(*cred) ;
    return getsockopt(fd, SOL_SOCKET, SO_PEERCRED, cred, &len) ;
}