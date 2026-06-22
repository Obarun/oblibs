/*
 * file_read.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/types.h>
#include <stddef.h>
#include <fcntl.h>

#include <oblibs/files.h>
#include <oblibs/io.h>

ssize_t file_read(char const *file, char *buf, size_t n)
{
    return io_readnclose(io_open(file, O_RDONLY | O_CLOEXEC), buf, n) ;
}
