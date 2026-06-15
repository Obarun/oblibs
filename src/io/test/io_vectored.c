/*
 * io_vectored.c
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

/* Tests for the vectored io_*v primitives. The big-transfer-over-a-pipe case
 * is the important one: the data dwarfs the kernel pipe buffer, so writev()
 * returns short repeatedly and io_allwritev must walk through the iovec array
 * (including mid-iovec splits) -- same for io_allreadv on the reader side. A
 * child process drains the pipe so the blocking parent never deadlocks. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <oblibs/io.h>

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

#define BIG (256 * 1024)

static void fill_pattern(unsigned char *b, size_t len)
{
    for (size_t i = 0 ; i < len ; i++) b[i] = (unsigned char)(i & 0xff) ;
}

/* Parent gathers BIG bytes from 4 iovecs into the pipe; child scatters them
 * back into 3 iovecs. Both sides exercise the partial-advance loop because the
 * transfer is far larger than the pipe buffer. */
static void test_big_pipe_roundtrip(void)
{
    TEST_START("test_big_pipe_roundtrip") ;

    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;

    pid_t pid = fork() ;
    TEST_ASSERT(pid >= 0, "fork failed") ;

    if (pid == 0) {
        /* child: reader */
        close(p[1]) ;
        unsigned char *in = malloc(BIG) ;
        if (!in) _exit(2) ;
        struct iovec v[3] = {
            { in,               100000 },
            { in + 100000,      100000 },
            { in + 200000,      BIG - 200000 },
        } ;
        size_t got = io_allreadv(p[0], v, 3) ;
        if (got != BIG) _exit(3) ;
        for (size_t i = 0 ; i < BIG ; i++)
            if (in[i] != (unsigned char)(i & 0xff)) _exit(4) ;
        free(in) ;
        close(p[0]) ;
        _exit(0) ;
    }

    /* parent: writer */
    close(p[0]) ;
    unsigned char *out = malloc(BIG) ;
    TEST_ASSERT(out != NULL, "malloc failed") ;
    fill_pattern(out, BIG) ;
    struct iovec v[4] = {
        { out,              70000 },
        { out + 70000,      70000 },
        { out + 140000,     70000 },
        { out + 210000,     BIG - 210000 },
    } ;
    size_t put = io_allwritev(p[1], v, 4) ;
    TEST_ASSERT(put == BIG, "io_allwritev did not write everything") ;
    close(p[1]) ;
    free(out) ;

    int status ;
    TEST_ASSERT(waitpid(pid, &status, 0) == pid, "waitpid failed") ;
    TEST_ASSERT(WIFEXITED(status), "child did not exit normally") ;
    TEST_ASSERT(WEXITSTATUS(status) == 0, "child reported a mismatch") ;

    TEST_END() ;
}

/* Single io_writev / io_readv on small data that fits the pipe in one go. */
static void test_single(void)
{
    TEST_START("test_single") ;

    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;

    char a[] = "hello " ;
    char b[] = "world" ;
    struct iovec wv[2] = { { a, 6 }, { b, 5 } } ;
    ssize_t w = io_writev(p[1], wv, 2) ;
    TEST_ASSERT(w == 11, "io_writev short") ;

    char buf[11] ;
    struct iovec rv[2] = { { buf, 6 }, { buf + 6, 5 } } ;
    ssize_t r = io_readv(p[0], rv, 2) ;
    TEST_ASSERT(r == 11, "io_readv short") ;
    TEST_ASSERT(memcmp(buf, "hello world", 11) == 0, "content mismatch") ;

    close(p[0]) ; close(p[1]) ;
    TEST_END() ;
}

/* Degenerate iovcnt must be a no-op returning 0. */
static void test_empty(void)
{
    TEST_START("test_empty") ;

    struct iovec v[1] = { { 0, 0 } } ;
    TEST_ASSERT(io_allwritev(1, v, 0) == 0, "allwritev(0) should be 0") ;
    TEST_ASSERT(io_allreadv(0, v, 0) == 0, "allreadv(0) should be 0") ;

    TEST_END() ;
}

/* A zero-length iovec in the middle of a non-empty array must be skipped (not
 * corrupt the stream nor loop), and a negative iovcnt is degenerate like 0. */
static void test_empty_segment_and_negative(void)
{
    TEST_START("test_empty_segment_and_negative") ;

    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;

    char a[] = "abc", b[] = "def" ;
    struct iovec wv[3] = { { a, 3 }, { a, 0 }, { b, 3 } } ;
    TEST_ASSERT(io_allwritev(p[1], wv, 3) == 6, "allwritev must skip the empty segment") ;

    char buf[6] ;
    struct iovec rv[3] = { { buf, 3 }, { buf, 0 }, { buf + 3, 3 } } ;
    TEST_ASSERT(io_allreadv(p[0], rv, 3) == 6, "allreadv must skip the empty segment") ;
    TEST_ASSERT(memcmp(buf, "abcdef", 6) == 0, "content mismatch") ;

    struct iovec v1[1] = { { buf, 3 } } ;
    TEST_ASSERT(io_allwritev(p[1], v1, -1) == 0, "allwritev(-1) should be 0") ;
    TEST_ASSERT(io_allreadv(p[0], v1, -1) == 0, "allreadv(-1) should be 0") ;

    close(p[0]) ; close(p[1]) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== io vectored test suite ===\n\n") ;

    test_single() ;
    test_big_pipe_roundtrip() ;
    test_empty() ;
    test_empty_segment_and_negative() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
