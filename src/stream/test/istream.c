/*
 * istream.c
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

/* Tests for the istream buffered reader. A tiny backing buffer forces repeated
 * refills from the fd, so reads that span the buffer boundary and partial reads
 * at EOF are exercised deterministically. The source is a temp file primed with
 * known bytes, read back in small chunks and compared. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/io.h>
#include <oblibs/stream.h>
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

/* Write `len` bytes to a fresh temp file, rewind, return the fd. */
static int primed_fd(char const *data, size_t len)
{
    char path[] = "/tmp/oblibs_istream_XXXXXX" ;
    int fd = mkstemp(path) ;
    TEST_ASSERT(fd >= 0, "mkstemp failed") ;
    TEST_ASSERT(write(fd, data, len) == (ssize_t)len, "write failed") ;
    TEST_ASSERT(lseek(fd, 0, SEEK_SET) == 0, "lseek failed") ;
    file_tryunlink(path) ;
    return fd ;
}

/* Reads spanning the buffer boundary deliver the full content in order, and a
 * read past the end returns a short count (EOF), then 0. */
static void test_get_across_refills(void)
{
    TEST_START("test_get_across_refills") ;

    char data[50] ;
    for (size_t i = 0 ; i < sizeof data ; i++) data[i] = (char)('A' + (i % 26)) ;
    int fd = primed_fd(data, sizeof data) ;

    istream o ;
    char storage[8] ;                  /* capacity 7 -> many refills for 50 bytes */
    TEST_ASSERT(istream_init(&o, fd, storage, sizeof storage), "istream_init") ;

    /* read in chunks of 7 (== capacity), then a final partial chunk */
    char got[50] ;
    size_t off = 0 ;
    while (off < sizeof data) {
        ssize_t r = istream_get(&o, got + off, 7) ;
        TEST_ASSERT(r > 0, "get returned no progress") ;
        off += (size_t)r ;
    }
    TEST_ASSERT(off == sizeof data, "total byte count") ;
    TEST_ASSERT(memcmp(got, data, sizeof data) == 0, "content matches") ;

    /* at EOF: a request for more returns 0 */
    char c ;
    TEST_ASSERT(istream_get(&o, &c, 1) == 0, "EOF returns 0") ;

    close(fd) ;
    TEST_END() ;
}

/* A single get for more than the whole file returns the partial count at EOF. */
static void test_get_short_at_eof(void)
{
    TEST_START("test_get_short_at_eof") ;

    char const data[] = "hello" ;
    int fd = primed_fd(data, sizeof data - 1) ;

    istream o ;
    char storage[16] ;
    TEST_ASSERT(istream_init(&o, fd, storage, sizeof storage), "istream_init") ;

    char got[32] ;
    ssize_t r = istream_get(&o, got, sizeof got) ;
    TEST_ASSERT(r == (ssize_t)(sizeof data - 1), "short count at EOF") ;
    TEST_ASSERT(memcmp(got, data, sizeof data - 1) == 0, "content matches") ;

    close(fd) ;
    TEST_END() ;
}

/* Byte-at-a-time reads pull through the buffer correctly. */
static void test_get_one_by_one(void)
{
    TEST_START("test_get_one_by_one") ;

    char const data[] = "0123456789" ;
    int fd = primed_fd(data, sizeof data - 1) ;

    istream o ;
    char storage[4] ;
    TEST_ASSERT(istream_init(&o, fd, storage, sizeof storage), "istream_init") ;

    for (size_t i = 0 ; i < sizeof data - 1 ; i++) {
        char c ;
        TEST_ASSERT(istream_get(&o, &c, 1) == 1, "one byte read") ;
        TEST_ASSERT(c == data[i], "byte value") ;
    }
    char c ;
    TEST_ASSERT(istream_get(&o, &c, 1) == 0, "EOF returns 0") ;

    close(fd) ;
    TEST_END() ;
}

/* The non-blocking building blocks: istream_getnofill never touches the fd, and
 * istream_fill reports the three outcomes distinctly -- bytes (>0), EOF (0), and
 * would-block (-1 / EAGAIN). This is what drives an event-loop fd cleanly, with
 * EOF and "nothing right now" kept apart (unlike the blocking istream_get). */
static void test_fill_getnofill_nonblock(void)
{
    TEST_START("test_fill_getnofill_nonblock") ;

    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;
    TEST_ASSERT(fcntl(p[0], F_SETFL, O_NONBLOCK) == 0, "set non-blocking failed") ;

    istream o ;
    char storage[16] ;
    TEST_ASSERT(istream_init(&o, p[0], storage, sizeof storage), "istream_init") ;

    char c ;
    /* nothing buffered: getnofill yields 0 and never reads the fd */
    TEST_ASSERT(istream_getnofill(&o, &c, 1) == 0, "getnofill on empty buffer -> 0") ;

    /* nothing on the fd yet: fill reports would-block as -1/EAGAIN (NOT EOF) */
    errno = 0 ;
    ssize_t r = istream_fill(&o) ;
    TEST_ASSERT(r == -1 && error_isagain(errno), "fill on empty non-blocking fd -> -1/EAGAIN") ;

    /* feed two bytes: one fill pulls them in, getnofill drains them in order */
    TEST_ASSERT(write(p[1], "ab", 2) == 2, "write to pipe") ;
    TEST_ASSERT(istream_fill(&o) == 2, "fill pulls the 2 bytes") ;
    TEST_ASSERT(istream_getnofill(&o, &c, 1) == 1 && c == 'a', "first byte") ;
    TEST_ASSERT(istream_getnofill(&o, &c, 1) == 1 && c == 'b', "second byte") ;
    TEST_ASSERT(istream_getnofill(&o, &c, 1) == 0, "buffer drained -> 0") ;

    /* would-block again, distinctly from EOF */
    errno = 0 ;
    TEST_ASSERT(istream_fill(&o) == -1 && error_isagain(errno), "would-block again") ;

    /* close the write end: now fill reports a real EOF (0), not -1 */
    close(p[1]) ;
    TEST_ASSERT(istream_fill(&o) == 0, "EOF after writer closed -> 0") ;

    close(p[0]) ;
    TEST_END() ;
}

/* istream_fill on a full buffer cannot read: it returns -1/ENOBUFS rather than
 * mistaking a zero-iovec read for EOF. */
static void test_fill_full_buffer(void)
{
    TEST_START("test_fill_full_buffer") ;

    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;
    TEST_ASSERT(fcntl(p[0], F_SETFL, O_NONBLOCK) == 0, "set non-blocking failed") ;

    istream o ;
    char storage[4] ;                 /* capacity 3 */
    TEST_ASSERT(istream_init(&o, p[0], storage, sizeof storage), "istream_init") ;

    TEST_ASSERT(write(p[1], "xyz", 3) == 3, "fill the capacity") ;
    TEST_ASSERT(istream_fill(&o) == 3, "fill to capacity") ;

    errno = 0 ;
    TEST_ASSERT(istream_fill(&o) == -1 && errno == ENOBUFS, "fill on full buffer -> ENOBUFS") ;

    close(p[0]) ; close(p[1]) ;
    TEST_END() ;
}

/* istream_getall reads a fixed-size record, resumably: on a non-blocking fd a
 * fragmented write is picked up across calls (return 0, *w holds progress), it
 * returns 1 once complete, and -1/EPIPE if the peer closes before len. */
static void test_getall_resumable(void)
{
    TEST_START("test_getall_resumable") ;

    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;
    TEST_ASSERT(fcntl(p[0], F_SETFL, O_NONBLOCK) == 0, "set non-blocking failed") ;

    istream o ;
    char storage[16] ;
    TEST_ASSERT(istream_init(&o, p[0], storage, sizeof storage), "istream_init") ;

    char rec[5] ;
    size_t w = 0 ;

    /* nothing yet: would-block, no progress */
    errno = 0 ;
    TEST_ASSERT(istream_getall(&o, rec, 5, &w) == 0, "would-block -> 0") ;
    TEST_ASSERT(w == 0, "no progress yet") ;

    /* first fragment "ab": still incomplete, progress recorded */
    TEST_ASSERT(write(p[1], "ab", 2) == 2, "write fragment 1") ;
    TEST_ASSERT(istream_getall(&o, rec, 5, &w) == 0, "still would-block -> 0") ;
    TEST_ASSERT(w == 2, "2 bytes of progress") ;

    /* rest "cde": now complete */
    TEST_ASSERT(write(p[1], "cde", 3) == 3, "write fragment 2") ;
    TEST_ASSERT(istream_getall(&o, rec, 5, &w) == 1, "complete -> 1") ;
    TEST_ASSERT(w == 5 && memcmp(rec, "abcde", 5) == 0, "record assembled") ;

    /* EOF before a full next record */
    char rec2[4] ;
    size_t w2 = 0 ;
    TEST_ASSERT(write(p[1], "zz", 2) == 2, "write short") ;
    close(p[1]) ;
    errno = 0 ;
    TEST_ASSERT(istream_getall(&o, rec2, 4, &w2) == -1 && errno == EPIPE, "EOF before len -> -1/EPIPE") ;
    TEST_ASSERT(w2 == 2 && memcmp(rec2, "zz", 2) == 0, "partial kept on EOF") ;

    close(p[0]) ;
    TEST_END() ;
}

/* istream_getall must assemble an exact record larger than the buffer capacity
 * (many fill/drain cycles), and report a clean EOF afterwards. Blocking fd: no
 * EAGAIN, so the only outcomes are complete (1) and EOF (-1/EPIPE). */
static void test_getall_small_buffer_blocking(void)
{
    TEST_START("test_getall_small_buffer_blocking") ;

    char const data[] = "0123456789" ;              /* 10 bytes */
    int fd = primed_fd(data, sizeof data - 1) ;

    istream o ;
    char storage[4] ;                               /* capacity 3 < 10 */
    TEST_ASSERT(istream_init(&o, fd, storage, sizeof storage), "istream_init") ;

    char buf[10] ;
    size_t w = 0 ;
    TEST_ASSERT(istream_getall(&o, buf, 10, &w) == 1, "complete across many refills") ;
    TEST_ASSERT(w == 10 && memcmp(buf, data, 10) == 0, "record assembled in order") ;

    /* nothing left: a further getall hits EOF cleanly */
    char b2[4] ;
    size_t w2 = 0 ;
    errno = 0 ;
    TEST_ASSERT(istream_getall(&o, b2, 4, &w2) == -1 && errno == EPIPE, "EOF -> -1/EPIPE") ;
    TEST_ASSERT(w2 == 0, "no bytes delivered at EOF") ;

    /* already-complete request (*w == len) returns 1 without touching the fd */
    size_t w3 = 3 ;
    TEST_ASSERT(istream_getall(&o, b2, 3, &w3) == 1, "*w==len -> 1, no read") ;

    /* invalid: *w > len -> EINVAL */
    size_t w4 = 5 ;
    errno = 0 ;
    TEST_ASSERT(istream_getall(&o, b2, 3, &w4) == -1 && errno == EINVAL, "*w>len -> EINVAL") ;

    close(fd) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== istream test suite ===\n\n") ;

    test_get_across_refills() ;
    test_get_short_at_eof() ;
    test_get_one_by_one() ;
    test_fill_getnofill_nonblock() ;
    test_fill_full_buffer() ;
    test_getall_resumable() ;
    test_getall_small_buffer_blocking() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
