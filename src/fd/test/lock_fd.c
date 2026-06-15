/*
 * lock_fd.c
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

/* Non-regression test for the oblibs-native lock_fd (flock-based replacement for
 * skalibs fd_lock). flock locks are owned by the open file description, so two
 * independent open()s of the same file contend even within one process -- which
 * is what lets this run single-threaded. Covers: exclusive lock taken (1),
 * non-blocking contention reported as 0, release on close, shared locks
 * coexisting, errno preserved on success, and -1 on a bad fd. */

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

static char tpath[] = "/tmp/oblibs_lockfd_XXXXXX" ;

/* Exclusive non-blocking lock: first taker gets it (1), a second independent
 * descriptor contends (0); closing the first releases it for the second. */
static void test_exclusive_contention(void)
{
    TEST_START("test_exclusive_contention") ;

    int a = open(tpath, O_WRONLY) ;
    TEST_ASSERT(a >= 0, "open a failed") ;
    int b = open(tpath, O_WRONLY) ;
    TEST_ASSERT(b >= 0, "open b failed") ;

    errno = EXDEV ;   /* sentinel: a successful lock must restore it */
    TEST_ASSERT(lock_fd(a, 1, 1) == 1, "exclusive lock on a should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    TEST_ASSERT(lock_fd(b, 1, 1) == 0, "b must see the lock as contended (0)") ;

    close(a) ;        /* releases the description's lock */
    TEST_ASSERT(lock_fd(b, 1, 1) == 1, "b should lock once a is closed") ;

    close(b) ;
    TEST_END() ;
}

/* Two shared (read) locks coexist without contention. */
static void test_shared_coexist(void)
{
    TEST_START("test_shared_coexist") ;

    int a = open(tpath, O_RDONLY) ;
    int b = open(tpath, O_RDONLY) ;
    TEST_ASSERT(a >= 0 && b >= 0, "open failed") ;

    TEST_ASSERT(lock_fd(a, 0, 1) == 1, "shared lock a should succeed") ;
    TEST_ASSERT(lock_fd(b, 0, 1) == 1, "shared lock b should coexist") ;

    close(a) ;
    close(b) ;
    TEST_END() ;
}

/* A bad descriptor is a hard error (-1), never a contention (0). */
static void test_bad_fd(void)
{
    TEST_START("test_bad_fd") ;

    errno = 0 ;
    TEST_ASSERT(lock_fd(-1, 1, 1) == -1, "lock on bad fd must return -1") ;
    TEST_ASSERT(errno == EBADF, "bad fd should set EBADF") ;

    TEST_END() ;
}

int main(void)
{
    int fd = mkstemp(tpath) ;
    TEST_ASSERT(fd >= 0, "mkstemp failed") ;
    close(fd) ;

    printf("=== lock_fd test suite ===\n\n") ;

    test_exclusive_contention() ;
    test_shared_coexist() ;
    test_bad_fd() ;

    unlink(tpath) ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
