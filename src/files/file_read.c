/*
 * file_read.c
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

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

int file_read(strbuf *sb, char const *dir, char const *file)
{
    _alloc_strbuf_(p, strlen(dir) + 1 + strlen(file)) ;
    if (!auto_strbuf(&p, dir, "/", file))
        return 0 ;
    return strbuf_read_file(sb, p.s) ;
}
