/*
 * file_write_atomic.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

int file_write_atomic(char const *file, char const *content, size_t len)
{
    size_t flen = strlen(file) ;
    char tmp[flen + 7 + 1] ;                 /* "<file>.XXXXXX" + NUL */
    auto_strings(tmp, file, ".XXXXXX") ;

    int fd = mkstemp(tmp) ;                   /* temp in the same dir -> atomic rename */
    if (fd < 0)
        return 0 ;

    if (fchmod(fd, 0644) < 0) {               /* mkstemp creates 0600 */
        close_fd(fd) ;
        file_tryunlink(tmp) ;
        return 0 ;
    }

    if (!io_writenclose(fd, content, len)) {  /* writes then closes fd */
        file_tryunlink(tmp) ;
        return 0 ;
    }

    if (rename(tmp, file) < 0) {
        file_tryunlink(tmp) ;
        return 0 ;
    }

    return 1 ;
}
