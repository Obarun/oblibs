/*
 * dir_close.c
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

/* Test for dir_close: it must close the directory stream while leaving errno
 * untouched (the whole point over a bare closedir on a cleanup path). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <oblibs/directory.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)) ; \
            exit(1) ; \
        } \
    } while(0)

int main(void)
{
    printf("Running dir_close...") ;

    DIR *d = opendir("/") ;
    TEST_ASSERT(d != NULL, "opendir / failed") ;
    int fd = dirfd(d) ;
    TEST_ASSERT(fd >= 0, "dirfd failed") ;

    errno = EXDEV ;                 /* sentinel: must survive dir_close */
    dir_close(d) ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved across dir_close") ;

    /* the errno check is done; now probe that the fd is really closed */
    TEST_ASSERT(fcntl(fd, F_GETFD) == -1 && errno == EBADF, "dir fd was not closed") ;

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
