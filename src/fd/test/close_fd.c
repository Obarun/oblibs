/*
 * close_fd.c
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

/* Non-regression test for the oblibs-native close_fd, the drop-in replacement
 * for skalibs' close_fd. oblibs keeps the very same symbol name, so the two
 * implementations cannot be linked side by side and called distinctly in one
 * translation unit -- the test therefore pins down skalibs' exact contract
 * instead: the descriptor is really closed, and errno is preserved across the
 * call (read before close(), restored after), close()'s own outcome ignored.
 * Covers the success path, the already-invalid-fd path, and errno == 0. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/fd.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)) ; \
            exit(1) ; \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

/* Is fd a closed/invalid descriptor? fcntl clobbers errno, so callers must
 * save anything they care about before invoking this. */
static int fd_is_closed(int fd)
{
    return fcntl(fd, F_GETFD) == -1 && errno == EBADF ;
}

/* A valid open fd is actually closed, and a non-zero errno set beforehand is
 * left untouched (the whole point of close_fd over a plain close()). */
static void test_success_preserves_errno(void)
{
    TEST_START("test_success_preserves_errno") ;

    int fd = open("/dev/null", O_RDONLY) ;
    TEST_ASSERT(fd >= 0, "open /dev/null failed") ;

    errno = EXDEV ;            /* sentinel: must survive the close */
    close_fd(fd) ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved across close_fd") ;

    /* now the errno check is done, we may clobber it to probe the fd */
    TEST_ASSERT(fd_is_closed(fd), "fd was not actually closed") ;

    TEST_END() ;
}

/* Closing an already-invalid fd makes the underlying close() fail with EBADF,
 * yet close_fd must still restore the caller's errno rather than leak EBADF. */
static void test_invalid_fd_preserves_errno(void)
{
    TEST_START("test_invalid_fd_preserves_errno") ;

    int fd = open("/dev/null", O_RDONLY) ;
    TEST_ASSERT(fd >= 0, "open /dev/null failed") ;
    TEST_ASSERT(close(fd) == 0, "manual close failed") ;   /* fd now invalid */

    errno = EXDEV ;
    close_fd(fd) ;             /* internal close() fails EBADF, must be hidden */
    TEST_ASSERT(errno == EXDEV, "errno not preserved on already-closed fd") ;

    errno = EXDEV ;
    close_fd(-1) ;            /* clearly invalid */
    TEST_ASSERT(errno == EXDEV, "errno not preserved on close_fd(-1)") ;

    TEST_END() ;
}

/* errno == 0 going in must stay 0 going out (no spurious error leaked). */
static void test_zero_errno_stays_zero(void)
{
    TEST_START("test_zero_errno_stays_zero") ;

    int fd = open("/dev/null", O_RDONLY) ;
    TEST_ASSERT(fd >= 0, "open /dev/null failed") ;

    errno = 0 ;
    close_fd(fd) ;
    TEST_ASSERT(errno == 0, "errno should remain 0") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== close_fd test suite ===\n\n") ;

    test_success_preserves_errno() ;
    test_invalid_fd_preserves_errno() ;
    test_zero_errno_stays_zero() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
