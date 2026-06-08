/*
 * remap_fds.c
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

/* Non-regression test for the oblibs-native remap_fds, drop-in replacement for
 * skalibs' fd_move2. Pins down the contract: both descriptors land on their
 * targets even when the pairs overlap (the crossing/swap case, where to1 is
 * from2 and must be parked with dup), the sources are closed, errno is
 * preserved on success, and the degenerate argument combinations are rejected
 * with EINVAL. Each pair carries a distinct byte so the final wiring is checked
 * by reading through the target descriptors, not just by their validity. */

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

/* A readable fd holding exactly the byte `c`, rewound to the start. */
static int byte_fd(char c)
{
    char path[] = "/tmp/oblibs_move2_XXXXXX" ;
    int fd = mkstemp(path) ;
    TEST_ASSERT(fd >= 0, "mkstemp failed") ;
    unlink(path) ;
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

/* Disjoint pairs: from1->to1 and from2->to2, both sources closed, errno kept. */
static void test_two_disjoint(void)
{
    TEST_START("test_two_disjoint") ;

    int from1 = byte_fd('A') ;
    int from2 = byte_fd('B') ;
    /* reserve two free, distinct target numbers */
    int to1 = dup(from1) ; int to2 = dup(from1) ;
    TEST_ASSERT(to1 >= 0 && to2 >= 0, "dup reservations failed") ;
    close(to1) ; close(to2) ;

    errno = EXDEV ;                 /* sentinel for the success path */
    int r = remap_fds(to1, from1, to2, from2) ;
    TEST_ASSERT(r == 0, "remap_fds should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    TEST_ASSERT(read_byte(to1) == 'A', "to1 should carry from1's description") ;
    TEST_ASSERT(read_byte(to2) == 'B', "to2 should carry from2's description") ;
    TEST_ASSERT(is_closed(from1), "from1 should be closed") ;
    TEST_ASSERT(is_closed(from2), "from2 should be closed") ;

    close(to1) ; close(to2) ;
    TEST_END() ;
}

/* The crossing case: swap two descriptors (to1 == from2 and to2 == from1).
 * This is what 66-shutdownd relies on; it exercises the dup() parking path. */
static void test_swap(void)
{
    TEST_START("test_swap") ;

    int a = byte_fd('X') ;
    int b = byte_fd('Y') ;

    /* swap: a's description must end up reachable through b and vice versa */
    int r = remap_fds(b, a, a, b) ;
    TEST_ASSERT(r == 0, "remap_fds swap should succeed") ;

    TEST_ASSERT(read_byte(b) == 'X', "b should now carry a's description") ;
    TEST_ASSERT(read_byte(a) == 'Y', "a should now carry b's description") ;
    TEST_ASSERT(fcntl(a, F_GETFD) >= 0, "a still valid") ;
    TEST_ASSERT(fcntl(b, F_GETFD) >= 0, "b still valid") ;

    close(a) ; close(b) ;
    TEST_END() ;
}

/* Degenerate inputs are rejected with EINVAL. */
static void test_einval(void)
{
    TEST_START("test_einval") ;

    int from1 = byte_fd('A') ;
    int from2 = byte_fd('B') ;

    /* same source, distinct targets -> EINVAL */
    errno = 0 ;
    TEST_ASSERT(remap_fds(from1 + 100, from1, from1 + 101, from1) == -1, "same source must fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL (same source)") ;

    /* same target -> EINVAL */
    errno = 0 ;
    TEST_ASSERT(remap_fds(from1 + 100, from1, from1 + 100, from2) == -1, "same target must fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL (same target)") ;

    close(from1) ; close(from2) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== remap_fds test suite ===\n\n") ;

    test_two_disjoint() ;
    test_swap() ;
    test_einval() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
