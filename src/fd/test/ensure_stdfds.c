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
 * /dev/null with the direction matching its role (fd 0 read-only, so a read
 * returns EOF; fd 1 and fd 2 write-only, so a write succeeds). The test saves
 * and restores every descriptor it disturbs so its own stdout/stderr survive.
 * A final case starves the descriptor table to drive the error return (0). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

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

/* fd 1 and fd 2 are reopened write-only: ensure_stdfds must make them writable.
 * We dup the descriptor away first (so the real stdout/stderr is preserved),
 * close it, sanitize, then prove a write of one byte succeeds. A read-only
 * reopen (the O_WRONLY->O_RDONLY mutation) would make write() fail with EBADF.
 * Diagnostics and restoration go through the saved descriptor, never through the
 * fd under test, which is closed during the critical section. */
static void test_reopens_writable(int fd)
{
    int saved = dup(fd) ;               /* keep the real stdout/stderr alive */
    if (saved < 0) { dprintf(2, "FAIL: dup(%d) failed\n", fd) ; exit(1) ; }

    if (close(fd) != 0) { dprintf(saved, "FAIL: close(%d) failed\n", fd) ; exit(1) ; }

    errno = 0 ;
    if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) {
        dprintf(saved, "FAIL: fd %d not closed before sanitize\n", fd) ; exit(1) ;
    }

    if (ensure_stdfds() != 1) {
        dprintf(saved, "FAIL: ensure_stdfds() != 1 reopening fd %d\n", fd) ; exit(1) ;
    }
    if (fcntl(fd, F_GETFD) == -1) {
        dprintf(saved, "FAIL: fd %d not reopened\n", fd) ; exit(1) ;
    }

    errno = 0 ;
    ssize_t w = write(fd, "x", 1) ;     /* O_WRONLY -> 1 ; O_RDONLY -> -1/EBADF */
    if (w != 1) {
        dprintf(saved, "FAIL: fd %d not writable after sanitize (write=%zd errno=%d)\n",
                fd, w, errno) ;
        exit(1) ;
    }

    if (dup2(saved, fd) != fd) { dprintf(saved, "FAIL: restore fd %d failed\n", fd) ; exit(1) ; }
    close(saved) ;
}

static void test_reopens_writable_stdout(void)
{
    TEST_START("test_reopens_writable_stdout") ;
    test_reopens_writable(1) ;
    TEST_END() ;
}

static void test_reopens_writable_stderr(void)
{
    TEST_START("test_reopens_writable_stderr") ;
    test_reopens_writable(2) ;
    TEST_END() ;
}

/* Error return: ensure_open hits its error path when io_open("/dev/null") fails.
 * We close fd 0 and starve the descriptor table down to fd 0 itself, so the
 * lowest free slot (0) lies AT the soft limit and open() is refused with EMFILE.
 * This is the only externally reachable path that makes ensure_stdfds return 0. */
static void test_error_returns_zero(void)
{
    TEST_START("test_error_returns_zero") ;

    struct rlimit old ;
    TEST_ASSERT(getrlimit(RLIMIT_NOFILE, &old) == 0, "getrlimit failed") ;

    int saved0 = dup(0) ;               /* preserve real stdin to restore later */
    TEST_ASSERT(saved0 >= 0, "dup(0) failed") ;

    close(0) ;                          /* fd 0 is the slot ensure_stdfds reopens */

    /* Soft limit 0: open() must return a fd strictly below the limit, but the
     * lowest free slot is 0 which is not < 0, so the kernel refuses with EMFILE. */
    struct rlimit lim = old ;
    lim.rlim_cur = 0 ;
    TEST_ASSERT(setrlimit(RLIMIT_NOFILE, &lim) == 0, "setrlimit failed") ;

    errno = 0 ;
    int r = ensure_stdfds() ;

    /* restore the limit and fd 0 before asserting so diagnostics can print */
    setrlimit(RLIMIT_NOFILE, &old) ;
    if (saved0 >= 0) { dup2(saved0, 0) ; close(saved0) ; }

    TEST_ASSERT(r == 0, "ensure_stdfds must return 0 when /dev/null cannot be opened") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== ensure_stdfds test suite ===\n\n") ;
    test_already_open() ;
    test_reopens_closed() ;
    test_reopens_writable_stdout() ;
    test_reopens_writable_stderr() ;
    test_error_returns_zero() ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
