/*
 * move_fd.c
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

/* Non-regression test for the oblibs-native move_fd, drop-in replacement for
 * skalibs' move_fd. Pins down the contract: the source is duplicated onto the
 * target then closed, errno is preserved on the success path, and the to==from
 * special case clears the close-on-exec flag instead of duplicating. */

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

static int is_closed(int fd)
{
    return fcntl(fd, F_GETFD) == -1 && errno == EBADF ;
}

/* from is duped onto a freed number then closed; the target becomes valid,
 * the source is closed, and a non-zero errno survives the success path. */
static void test_move_success(void)
{
    TEST_START("test_move_success") ;

    int from = open("/dev/null", O_RDONLY) ;
    TEST_ASSERT(from >= 0, "open /dev/null failed") ;

    int to = dup(from) ;            /* a distinct, valid number ... */
    TEST_ASSERT(to >= 0, "dup failed") ;
    TEST_ASSERT(close(to) == 0, "close(to) failed") ;   /* ... now free again */

    errno = EXDEV ;                 /* sentinel: success path must restore it */
    int r = move_fd(to, from) ;
    TEST_ASSERT(r == 0, "move_fd should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    TEST_ASSERT(fcntl(to, F_GETFD) >= 0, "target fd should be valid") ;
    TEST_ASSERT(is_closed(from), "source fd should be closed") ;

    close(to) ;
    TEST_END() ;
}

/* to == from: no dup, the close-on-exec flag is cleared (this exercises the
 * corrected uncloexec_fd under the hood). */
static void test_move_same_fd_clears_cloexec(void)
{
    TEST_START("test_move_same_fd_clears_cloexec") ;

    int fd = open("/dev/null", O_RDONLY | O_CLOEXEC) ;
    TEST_ASSERT(fd >= 0, "open /dev/null failed") ;
    TEST_ASSERT(fcntl(fd, F_GETFD) & FD_CLOEXEC, "cloexec should be set after O_CLOEXEC open") ;

    int r = move_fd(fd, fd) ;
    TEST_ASSERT(r == 0, "move_fd(fd, fd) should succeed") ;
    TEST_ASSERT(fcntl(fd, F_GETFD) >= 0, "fd should still be valid") ;
    TEST_ASSERT(!(fcntl(fd, F_GETFD) & FD_CLOEXEC), "cloexec should be cleared") ;

    close(fd) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== move_fd test suite ===\n\n") ;

    test_move_success() ;
    test_move_same_fd_clears_cloexec() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
