/*
 * io_read.c
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

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

ssize_t io_read(int fd, char *store, size_t len)
{
    ssize_t r ;
    do r = read(fd, store, len) ;
    while ((r < 0) && (errno == EINTR)) ;
    return r ;
}
