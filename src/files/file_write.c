/*
 * file_write.c
 *
 * Copyright (c) 2018-2026 Eric Vidal <eric@obarun.org>
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
#include <fcntl.h>

#include <oblibs/files.h>
#include <oblibs/io.h>

int file_write(char const *file, char const *content, size_t len)
{
    return io_writenclose(io_open_mode(file, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, 0666), content, len) ;
}
