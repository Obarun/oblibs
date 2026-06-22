/*
 * io_all.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <oblibs/io.h>

// Test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            printf("      Line %d: %s\n", __LINE__, #condition); \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)); \
            exit(1); \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

static void fill_pattern(char *buf, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        buf[i] = (char)(i & 0xff) ;
}

static int check_pattern(const char *buf, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        if ((unsigned char)buf[i] != (i & 0xff))
            return 0 ;
    return 1 ;
}

// Case 1: full transfer of a small buffer (fits in socket buffer, no blocking)
static void test_full_transfer(void)
{
    TEST_START("test_full_transfer") ;

    int fds[2] ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair failed") ;

    char out[256] ;
    char in[256] ;
    fill_pattern(out, sizeof out) ;
    memset(in, 0, sizeof in) ;

    size_t w = io_allwrite(fds[1], out, sizeof out) ;
    TEST_ASSERT(w == sizeof out, "io_allwrite short count") ;

    size_t r = io_allread(fds[0], in, sizeof in) ;
    TEST_ASSERT(r == sizeof in, "io_allread short count") ;
    TEST_ASSERT(check_pattern(in, sizeof in), "data mismatch") ;

    close(fds[0]) ; close(fds[1]) ;
    TEST_END() ;
}

// Case 2: large transfer forcing multiple short read/write iterations.
// A child drains the socket while the parent writes, so neither side deadlocks.
static void test_large_transfer(void)
{
    TEST_START("test_large_transfer") ;

    enum { N = 1024 * 1024 } ;   // 1 MiB > socket buffer => several loop iterations
    int fds[2] ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair failed") ;

    pid_t pid = fork() ;
    TEST_ASSERT(pid >= 0, "fork failed") ;

    if (!pid) {
        // child: reader
        close(fds[1]) ;
        char *in = malloc(N) ;
        if (!in) _exit(2) ;
        size_t r = io_allread(fds[0], in, N) ;
        int ok = (r == N) && check_pattern(in, N) ;
        free(in) ;
        close(fds[0]) ;
        _exit(ok ? 0 : 1) ;
    }

    // parent: writer
    close(fds[0]) ;
    char *out = malloc(N) ;
    TEST_ASSERT(out != NULL, "malloc failed") ;
    fill_pattern(out, N) ;
    size_t w = io_allwrite(fds[1], out, N) ;
    TEST_ASSERT(w == N, "io_allwrite short count on large buffer") ;
    free(out) ;
    close(fds[1]) ;

    int status ;
    TEST_ASSERT(waitpid(pid, &status, 0) == pid, "waitpid failed") ;
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "child reader failed") ;

    TEST_END() ;
}

// Case 3: premature EOF. Writer sends k < N bytes then closes.
static void test_eof_premature(void)
{
    TEST_START("test_eof_premature") ;

    enum { K = 100, N = 256 } ;
    int fds[2] ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair failed") ;

    char out[K] ;
    fill_pattern(out, sizeof out) ;
    size_t w = io_allwrite(fds[1], out, sizeof out) ;
    TEST_ASSERT(w == K, "io_allwrite short count") ;
    close(fds[1]) ;   // signal EOF to the reader

    char in[N] ;
    errno = 0 ;
    size_t r = io_allread(fds[0], in, N) ;
    TEST_ASSERT(r == K, "io_allread should return the bytes read before EOF") ;
    TEST_ASSERT(check_pattern(in, K), "data mismatch before EOF") ;

    // A subsequent read hits EOF straight away: 0 returned, errno == EPIPE
    errno = 0 ;
    size_t r2 = io_allread(fds[0], in, N) ;
    TEST_ASSERT(r2 == 0, "io_allread past EOF should return 0") ;
    TEST_ASSERT(errno == EPIPE, "io_allread past EOF should set errno to EPIPE") ;

    close(fds[0]) ;
    TEST_END() ;
}

// Case 4: write error. Peer is closed, so the write fails; errno is set.
static void test_write_error(void)
{
    TEST_START("test_write_error") ;

    int fds[2] ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair failed") ;

    close(fds[0]) ;   // close the read end => writing fails

    char out[256] ;
    fill_pattern(out, sizeof out) ;
    errno = 0 ;
    size_t w = io_allwrite(fds[1], out, sizeof out) ;
    TEST_ASSERT(w < sizeof out, "io_allwrite to a closed peer should be a short write") ;
    TEST_ASSERT(errno == EPIPE || errno == ECONNRESET, "errno should reflect the write error") ;

    close(fds[1]) ;
    TEST_END() ;
}

// Case 5: len == 0. The transfer loop never runs; both return 0 without I/O.
static void test_zero_length(void)
{
    TEST_START("test_zero_length") ;

    int fds[2] ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair failed") ;

    char b[1] ;
    TEST_ASSERT(io_allwrite(fds[1], b, 0) == 0, "io_allwrite len 0 -> 0") ;
    TEST_ASSERT(io_allread(fds[0], b, 0) == 0, "io_allread len 0 -> 0") ;

    close(fds[0]) ; close(fds[1]) ;
    TEST_END() ;
}

int main(void)
{
    // Writing to a closed peer raises SIGPIPE by default; ignore it so the
    // syscall returns -1/EPIPE instead of killing the test.
    signal(SIGPIPE, SIG_IGN) ;

    printf("=== io_allread / io_allwrite test suite ===\n\n") ;

    test_full_transfer() ;
    test_large_transfer() ;
    test_eof_premature() ;
    test_write_error() ;
    test_zero_length() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
