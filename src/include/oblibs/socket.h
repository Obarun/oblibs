/*
 * io.h
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
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
#include <sys/socket.h>
#include <oblibs/io.h>

#define SOCKETUNIX_BACKLOG SOMAXCONN
#define SOCKETUNIX_PATH_MAX 107 // 108 - 1

extern int socket_private(int domain, int type, int protocol, unsigned int flags) ;
extern int socket_setfd(int fd, int flags) ;
extern int socket_unsetfd(int fd, int flags) ;
#define socket_set_coe(fd) socket_setfd(fd, FD_CLOEXEC)
#define socket_set_nonblock(fd) io_setfl(fd, O_NONBLOCK)

#define socketunix_create(flags) socket_private(AF_UNIX, SOCK_STREAM, 0, flags)
#define socketunix_listen(socket, backlog) listen(socket, backlog)

extern int socketunix_bind(int fd, const char *path) ;
extern int socketunix_accept(int s, unsigned int flags) ;
extern int socketunix_connect(int fd, const char *path) ;
extern int socketunix_getucred(int fd, struct ucred *cred) ;

#endif
