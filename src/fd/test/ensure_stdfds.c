/*
 * ensure_stdfds.c
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

/* Non-regression test for the oblibs-native ensure_stdfds, drop-in replacement for
 * skalibs' ensure_stdfds. Pins down the contract: with 0/1/2 already open it
 * returns 1 and changes nothing; a closed standard descriptor is reopened on
 * /dev/null (read-only for fd 0, so a read returns EOF). The test acts on fd 0
 * only, saving and restoring it, so the test's own stdout (fd 1) is untouched. */

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

static void test_already_open(void)
{
    TEST_START("test_already_open") ;
    /* the test inherits 0/1/2 open -> nothing to do, returns 1 */
    TEST_ASSERT(ensure_stdfds() == 1, "all standard fds already open -> 1") ;
    TEST_END() ;
}

static void test_reopens_closed(void)
{
    TEST_START("test_reopens_closed") ;

    int saved = dup(0) ;                 /* -1 if fd 0 was already closed */
    close(0) ;                           /* fd 0 is now definitely closed */
    errno = 0 ;
    TEST_ASSERT(fcntl(0, F_GETFD) == -1 && errno == EBADF, "fd 0 closed before sanitize") ;

    TEST_ASSERT(ensure_stdfds() == 1, "sanitize succeeds") ;
    TEST_ASSERT(fcntl(0, F_GETFD) != -1, "fd 0 reopened") ;

    char c ;
    TEST_ASSERT(read(0, &c, 1) == 0, "fd 0 reads as /dev/null (immediate EOF)") ;

    if (saved >= 0) {                    /* restore the original stdin */
        TEST_ASSERT(dup2(saved, 0) == 0, "restore fd 0") ;
        close(saved) ;
    }
    TEST_END() ;
}

int main(void)
{
    printf("=== ensure_stdfds test suite ===\n\n") ;
    test_already_open() ;
    test_reopens_closed() ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
