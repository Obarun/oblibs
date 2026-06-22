/*
 * strbuf_read_file.c
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <oblibs/io.h>
#include <oblibs/strbuf.h>

int strbuf_read_file(strbuf *sb, char const *path)
{
    size_t base = sb->len ;
    int wasnull = !sb->s ;

    int fd = io_open(path, O_RDONLY | O_CLOEXEC) ;
    if (fd < 0) {
        errno = ENOENT ;
        goto err ;
    }

    struct stat st ;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd) ;
        errno = ENOENT ;
        goto err ;
    }

    size_t n = st.st_size ;

    if (!strbuf_reserve(sb, sb->len + n)) {
        close(fd) ;
        goto err ;
    }

    size_t got = io_allread(fd, sb->s + sb->len, n) ;
    sb->len += got ;
    close(fd) ;
    if (got < n) {
        errno = ENOENT ;
        goto err ;
    }

    if (!strbuf_uncounted(sb))
        goto err ;

    return 1 ;

    err:
        if (wasnull) strbuf_free(sb) ;
        else sb->len = base ;
        return 0 ;
}
