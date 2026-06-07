/*
 * ocdb_init.c
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

#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/cdb.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>
#include <oblibs/types.h>

ocdb const ocdb_zero = OCDB_ZERO ;

int ocdb_init_fromfd(ocdb *c, int fd)
{
    struct stat st ;
    char *map ;

    if (fstat(fd, &st) < 0)
        return 0 ;

    if ((uint64_t)st.st_size > UINT32_MAX)
        return (errno = EOVERFLOW, 0) ;

    map = mmap(0, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0) ;
    if (map == MAP_FAILED)
        return 0 ;

    c->map = map ;
    c->size = (uint32_t)st.st_size ;

    return 1 ;
}

int ocdb_init(ocdb *c, char const *path)
{
    // pourquoi pas l'utilisation des io_xx de oblibs ici et pourquoi O_CLOEXEC
    int fd = io_open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK) ;
    if (fd < 0)
        return 0 ;

    int r = ocdb_init_fromfd(c, fd) ;
    fd_close(fd) ;

    return r ;
}

int ocdb_init_at(ocdb *c, int dirfd, char const *path)
{
    int e = errno, fd, r ;
    do fd = openat(dirfd, path, O_RDONLY | O_CLOEXEC | O_NONBLOCK) ;
    while (fd == -1 && errno == EINTR) ;
    if (fd >= 0)
        errno = e ;
    else return 0 ;

    r = ocdb_init_fromfd(c, fd) ;
    fd_close(fd) ;

    return r ;
}

void ocdb_free(ocdb *c)
{
    ERRNO_SAFE ;
    if (c->map) {
        munmap((void *)c->map, c->size) ;
        c->map = 0 ;
    }
}
