/*
 * cloexec_fd.c
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

/* Non-regression test for cloexec_fd / uncloexec_fd. These act on the close-on-exec
 * descriptor flag (FD_CLOEXEC, via F_GETFD/F_SETFD) -- NOT on the file status
 * flags. The test verifies the flag is really set then cleared as observed
 * through F_GETFD, and that both return 0 (failure) on a bad fd. */

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

static void test_coe_uncoe_roundtrip(void)
{
    TEST_START("test_coe_uncoe_roundtrip") ;

    int fd = open("/dev/null", O_RDONLY) ;   /* no O_CLOEXEC -> flag clear */
    TEST_ASSERT(fd >= 0, "open /dev/null failed") ;
    TEST_ASSERT(!(fcntl(fd, F_GETFD) & FD_CLOEXEC), "cloexec should start clear") ;

    TEST_ASSERT(cloexec_fd(fd) == 1, "cloexec_fd should succeed") ;
    TEST_ASSERT(fcntl(fd, F_GETFD) & FD_CLOEXEC, "cloexec_fd must set FD_CLOEXEC") ;

    TEST_ASSERT(uncloexec_fd(fd) == 1, "uncloexec_fd should succeed") ;
    TEST_ASSERT(!(fcntl(fd, F_GETFD) & FD_CLOEXEC), "uncloexec_fd must clear FD_CLOEXEC") ;

    close(fd) ;
    TEST_END() ;
}

static void test_bad_fd(void)
{
    TEST_START("test_bad_fd") ;

    TEST_ASSERT(cloexec_fd(-1) == 0, "cloexec_fd(-1) should fail") ;
    TEST_ASSERT(uncloexec_fd(-1) == 0, "uncloexec_fd(-1) should fail") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== cloexec_fd / uncloexec_fd test suite ===\n\n") ;

    test_coe_uncoe_roundtrip() ;
    test_bad_fd() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
