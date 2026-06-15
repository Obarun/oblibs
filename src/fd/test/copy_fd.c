/*
 * copy_fd.c
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

/* Non-regression test for the oblibs-native copy_fd, drop-in replacement for
 * skalibs' copy_fd. Pins down the contract: to==from is rejected with EINVAL;
 * otherwise `to` ends up sharing `from`'s open file description (shared offset),
 * `from` stays open, `to` has no close-on-exec flag, and errno is preserved on
 * success. */

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

static char tpath[] = "/tmp/oblibs_fd_copy_XXXXXX" ;

static void test_reject_same(void)
{
    TEST_START("test_reject_same") ;
    errno = 0 ;
    TEST_ASSERT(copy_fd(3, 3) == -1, "to == from rejected") ;
    TEST_ASSERT(errno == EINVAL, "errno EINVAL") ;
    TEST_END() ;
}

static void test_copy(void)
{
    TEST_START("test_copy") ;

    int src = mkstemp(tpath) ;
    TEST_ASSERT(src >= 0, "mkstemp") ;
    file_tryunlink(tpath) ;

    /* obtain a known-free target number */
    int dst = dup(src) ;
    TEST_ASSERT(dst >= 0, "dup for a free number") ;
    TEST_ASSERT(close(dst) == 0, "free the target number") ;

    errno = EDOM ;                       /* sentinel: must survive a successful copy */
    int r = copy_fd(dst, src) ;
    TEST_ASSERT(r == dst, "copy_fd returns `to`") ;
    TEST_ASSERT(errno == EDOM, "errno preserved on success") ;

    TEST_ASSERT(fcntl(src, F_GETFD) != -1, "src still open") ;
    TEST_ASSERT(fcntl(dst, F_GETFD) != -1, "dst open") ;
    TEST_ASSERT(!(fcntl(dst, F_GETFD) & FD_CLOEXEC), "dst has no close-on-exec") ;

    /* shared open file description: the offset is shared between src and dst */
    TEST_ASSERT(write(src, "hello", 5) == 5, "write via src") ;
    TEST_ASSERT(lseek(dst, 0, SEEK_SET) == 0, "rewind via dst (shared offset)") ;
    char buf[6] = { 0 } ;
    TEST_ASSERT(read(dst, buf, 5) == 5 && !memcmp(buf, "hello", 5), "dst reads src's write") ;

    close(src) ;
    close(dst) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== copy_fd test suite ===\n\n") ;
    test_reject_same() ;
    test_copy() ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
