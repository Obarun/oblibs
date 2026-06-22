/*
 * file_read_at.c
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
#include <string.h>

#include <oblibs/files.h>
#include <oblibs/string.h>

ssize_t file_read_at(char const *dir, char const *name, char *buf, size_t n)
{
    char file[strlen(dir) + 1 + strlen(name) + 1] ;
    auto_strings(file, dir, "/", name) ;
    return file_read(file, buf, n) ;
}
