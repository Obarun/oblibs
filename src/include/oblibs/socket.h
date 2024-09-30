/*
 * io.h
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

#ifndef OB_SOCKET_H
#define OB_SOCKET_H

#include <fcntl.h>

#define SOCK_BACKLOG SOMAXCONN
#define socket_listen(socket, backlog) listen(socket, backlog)
#define socket_open(flags) socket(AF_UNIX, SOCK_STREAM | flags, 0)

extern int fd_set_flags(int fd, int flags) ;

#define socket_set_coe(fd) fd_set_flags(fd, FD_CLOEXEC)
#define socket_set_nonblock(fd) fd_set_flags(fd, O_NONBLOCK)

extern int socket_bind(int fd, const char *path) ;
extern int socket_accept(int fd) ;

#endif
