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
 * except according to the terms contained in the LICENSE file./
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

int main(void)
{
    printf("=== io_readnclose / io_writenclose test suite ===\n\n") ;

    test_roundtrip_and_close() ;
    test_fd_negative() ;
    test_short_read() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
