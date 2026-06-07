/*
 * chdir_fd.c
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

/* Non-regression test for the oblibs-native chdir_fd, drop-in replacement for
 * skalibs' chdir_fd. Pins down the contract: the cwd really moves to the
 * directory the fd refers to, errno is preserved on success, and a failure
 * (invalid fd) returns -1 with errno set (EBADF) -- i.e. not preserved. */

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

/* Opening "/" then cd'ing elsewhere, chdir_fd must bring us back to "/",
 * leaving a non-zero errno untouched. */
static void test_chdir_success(void)
{
    TEST_START("test_chdir_success") ;

    int root = open("/", O_RDONLY | O_NONBLOCK) ;
    TEST_ASSERT(root >= 0, "open / failed") ;

    TEST_ASSERT(chdir("/tmp") == 0, "chdir /tmp failed") ;

    errno = EXDEV ;                 /* sentinel: a successful call must keep it */
    int r = chdir_fd(root) ;
    TEST_ASSERT(r == 0, "chdir_fd should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    char cwd[4096] ;
    TEST_ASSERT(getcwd(cwd, sizeof cwd) != NULL, "getcwd failed") ;
    TEST_ASSERT(strcmp(cwd, "/") == 0, "cwd is not / after chdir_fd") ;

    close(root) ;
    TEST_END() ;
}

/* An invalid fd makes fchdir fail: -1 returned, errno = EBADF (the failure
 * errno is intentionally NOT masked by the save/restore). */
static void test_chdir_invalid_fd(void)
{
    TEST_START("test_chdir_invalid_fd") ;

    errno = 0 ;
    int r = chdir_fd(-1) ;
    TEST_ASSERT(r == -1, "chdir_fd(-1) should fail") ;
    TEST_ASSERT(errno == EBADF, "errno should be EBADF on invalid fd") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== chdir_fd test suite ===\n\n") ;

    test_chdir_success() ;
    test_chdir_invalid_fd() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
