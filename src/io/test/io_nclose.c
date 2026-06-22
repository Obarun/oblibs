/*
 * io_nclose.c
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

/* Tests for the io_readnclose / io_writenclose primitives: they transfer the
 * whole fd then always close it, and tolerate fd < 0 so they compose directly
 * with io_open(). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <oblibs/io.h>
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

/* write-all-and-close, then read-all-and-close: round-trip via a temp file,
 * and each call leaves its fd closed. */
static void test_roundtrip_and_close(void)
{
    TEST_START("test_roundtrip_and_close") ;

    char path[] = "/tmp/oblibs_nclose_XXXXXX" ;
    int wfd = mkstemp(path) ;
    TEST_ASSERT(wfd >= 0, "mkstemp failed") ;

    char const data[] = "abc\0def" ;        /* embeds a NUL: binary, explicit len */
    size_t len = sizeof data - 1 ;          /* 7 */

    TEST_ASSERT(io_writenclose(wfd, data, len) == 1, "writenclose should succeed") ;
    TEST_ASSERT(is_closed(wfd), "writenclose must close the fd") ;

    int rfd = io_open(path, O_RDONLY | O_CLOEXEC) ;
    TEST_ASSERT(rfd >= 0, "reopen failed") ;
    char buf[16] ;
    ssize_t r = io_readnclose(rfd, buf, sizeof buf) ;
    TEST_ASSERT(r == (ssize_t)len, "readnclose should return the byte count") ;
    TEST_ASSERT(memcmp(buf, data, len) == 0, "content mismatch") ;
    TEST_ASSERT(is_closed(rfd), "readnclose must close the fd") ;

    file_tryunlink(path) ;
    TEST_END() ;
}

/* fd < 0 is tolerated: composes with a failing io_open without a separate check. */
static void test_fd_negative(void)
{
    TEST_START("test_fd_negative") ;

    char c ;
    TEST_ASSERT(io_readnclose(-1, &c, 1) == -1, "readnclose(-1) -> -1") ;
    TEST_ASSERT(io_writenclose(-1, "x", 1) == 0, "writenclose(-1) -> 0") ;

    /* the intended composition: open failure propagates through the primitive */
    errno = 0 ;
    char buf[8] ;
    ssize_t r = io_readnclose(io_open("/oblibs/does/not/exist", O_RDONLY | O_CLOEXEC), buf, sizeof buf) ;
    TEST_ASSERT(r == -1, "composing with a failing open yields -1") ;

    TEST_END() ;
}

/* a file shorter than the buffer: readnclose returns the partial count (EOF is
 * not an error), not -1. */
static void test_short_read(void)
{
    TEST_START("test_short_read") ;

    char path[] = "/tmp/oblibs_nclose_XXXXXX" ;
    int wfd = mkstemp(path) ;
    TEST_ASSERT(wfd >= 0, "mkstemp failed") ;
    TEST_ASSERT(io_writenclose(wfd, "hello", 5) == 1, "write 5 bytes") ;

    char buf[64] ;
    ssize_t r = io_readnclose(io_open(path, O_RDONLY | O_CLOEXEC), buf, sizeof buf) ;
    TEST_ASSERT(r == 5, "short file -> partial count, not -1") ;
    TEST_ASSERT(memcmp(buf, "hello", 5) == 0, "content mismatch") ;

    file_tryunlink(path) ;
    TEST_END() ;
}

/* n == 0 reads nothing but still closes the fd; a genuine read error (not EOF)
 * yields -1 with errno preserved, fd still closed; a failed write yields 0. */
static void test_zero_and_errors(void)
{
    TEST_START("test_zero_and_errors") ;

    char buf[16] ;

    /* n == 0: returns 0, fd closed. */
    char path[] = "/tmp/oblibs_nclose_XXXXXX" ;
    int wfd = mkstemp(path) ;
    TEST_ASSERT(wfd >= 0, "mkstemp failed") ;
    TEST_ASSERT(io_writenclose(wfd, "data", 4) == 1, "seed file") ;
    int fd = io_open(path, O_RDONLY | O_CLOEXEC) ;
    TEST_ASSERT(fd >= 0, "open failed") ;
    TEST_ASSERT(io_readnclose(fd, buf, 0) == 0, "n==0 -> 0") ;
    TEST_ASSERT(is_closed(fd), "n==0 still closes the fd") ;
    file_tryunlink(path) ;

    /* real read error (not EOF): reading a directory fd fails with EISDIR. */
    int dfd = io_open("/tmp", O_RDONLY | O_CLOEXEC) ;
    TEST_ASSERT(dfd >= 0, "open dir failed") ;
    errno = 0 ;
    TEST_ASSERT(io_readnclose(dfd, buf, sizeof buf) == -1, "real read error -> -1") ;
    TEST_ASSERT(errno == EISDIR, "errno preserved from the failed read") ;
    TEST_ASSERT(is_closed(dfd), "fd closed even on read error") ;

    /* write failure: a pipe whose reader is gone makes the write fail (EPIPE);
     * the call returns 0 and still closes the fd. */
    signal(SIGPIPE, SIG_IGN) ;
    int pfd[2] ;
    TEST_ASSERT(pipe(pfd) == 0, "pipe failed") ;
    close(pfd[0]) ;
    TEST_ASSERT(io_writenclose(pfd[1], "hello", 5) == 0, "write to broken pipe -> 0") ;
    TEST_ASSERT(is_closed(pfd[1]), "fd closed even on write error") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== io_readnclose / io_writenclose test suite ===\n\n") ;

    test_roundtrip_and_close() ;
    test_fd_negative() ;
    test_short_read() ;
    test_zero_and_errors() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
