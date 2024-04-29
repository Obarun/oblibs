/*
 * io_open.c
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

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <oblibs/io.h>

ssize_t io_open(const char *path, unsigned int flags)
{
    ssize_t r ;
    do r = open(path, flags) ;
    while ((r < 0) && (errno == EINTR)) ;
    return r ;
}
