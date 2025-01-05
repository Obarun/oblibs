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

#ifndef OB_IO_H
#define OB_IO_H

#include <stddef.h>
#include <sys/types.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0x40000000
#endif

extern int io_set_flags(int fd, int flags) ;
extern ssize_t io_open(const char *path, unsigned int flags) ;
extern ssize_t io_read(char *store, int fd, size_t len) ;

#endif
