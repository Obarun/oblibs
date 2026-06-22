/*
 * file_write_at.c
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

#include <stddef.h>
#include <string.h>

#include <oblibs/files.h>
#include <oblibs/string.h>

int file_write_at(char const *dir, char const *name, char const *content, size_t len)
{
    char file[strlen(dir) + 1 + strlen(name) + 1] ;
    auto_strings(file, dir, "/", name) ;
    return file_write(file, content, len) ;
}
