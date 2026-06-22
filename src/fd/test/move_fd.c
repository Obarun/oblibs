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
 * except according to the terms contained in the LICENSE file.
 */

/* Non-regression test for the oblibs-native move_fd, drop-in replacement for
 * skalibs' move_fd. Pins down the contract: the source's description is
 * duplicated onto the target then the source is closed, errno is preserved on
 * the success path, a failed dup2 returns -1 without disturbing the source, and
 * the to==from special case clears the close-on-exec flag instead of
 * duplicating. The success path is checked by reading a distinctive byte back
 * through the target, not just by its validity. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/fd.h>
#include <oblibs/files.h>

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

/* A readable fd holding exactly the byte `c`, rewound to the start. */
static int byte_fd(char c)
{
    char path[] = "/tmp/oblibs_move_fd_XXXXXX" ;
    int fd = mkstemp(path) ;
    TEST_ASSERT(fd >= 0, "mkstemp failed") ;
    file_tryunlink(path) ;
    TEST_ASSERT(write(fd, &c, 1) == 1, "write failed") ;
    TEST_ASSERT(lseek(fd, 0, SEEK_SET) == 0, "lseek failed") ;
    return fd ;
}

static char read_byte(int fd)
{
    char c = 0 ;
    TEST_ASSERT(pread(fd, &c, 1, 0) == 1, "pread failed") ;
    return c ;
}

/* from is duped onto a freed number then closed; the target ends up carrying
 * from's description (proven by reading its distinctive byte), the source is
 * closed, and a non-zero errno survives the success path. */
static void test_move_success(void)
{
    TEST_START("test_move_success") ;

    int from = byte_fd('Z') ;       /* distinctive payload to identify the description */

    int to = dup(from) ;            /* a distinct, valid number ... */
    TEST_ASSERT(to >= 0, "dup failed") ;
    TEST_ASSERT(close(to) == 0, "close(to) failed") ;   /* ... now free again */

    errno = EXDEV ;                 /* sentinel: success path must restore it */
    int r = move_fd(to, from) ;
    TEST_ASSERT(r == 0, "move_fd should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    TEST_ASSERT(fcntl(to, F_GETFD) >= 0, "target fd should be valid") ;
    TEST_ASSERT(read_byte(to) == 'Z', "target must carry from's description") ;
    TEST_ASSERT(is_closed(from), "source fd should be closed") ;

    close(to) ;
    TEST_END() ;
}

/* A failed dup2 (bogus source -1) returns -1 and leaves the target untouched. */
static void test_move_failure(void)
{
    TEST_START("test_move_failure") ;

    int to = byte_fd('T') ;         /* valid, distinct target that must survive */

    errno = 0 ;
    int r = move_fd(to, -1) ;       /* dup2(-1, to) fails with EBADF */
    TEST_ASSERT(r == -1, "move_fd with invalid source should fail") ;
    TEST_ASSERT(errno == EBADF, "errno should be EBADF on dup2 failure") ;

    TEST_ASSERT(fcntl(to, F_GETFD) >= 0, "target fd should be untouched") ;
    TEST_ASSERT(read_byte(to) == 'T', "target description must be undisturbed") ;

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
    test_move_failure() ;
    test_move_same_fd_clears_cloexec() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
