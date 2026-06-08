/*
 * file_read.c
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

/* Tests for the path readers: file_read (read up to n bytes into a fixed buffer,
 * O_CLOEXEC, returns the byte count; a short file is a partial count, not an
 * error) and file_read_at (dir/name join). Round-tripped with file_write. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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

static char tdir[] = "/tmp/oblibs_fr_XXXXXX" ;

static void fill_pattern(char *buf, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        buf[i] = (char)(i & 0xff) ;
}

static void path_in(char *out, char const *name)
{
    size_t dl = strlen(tdir), nl = strlen(name) ;
    memcpy(out, tdir, dl) ;
    out[dl] = '/' ;
    memcpy(out + dl + 1, name, nl + 1) ;
}

/* read exactly n bytes when the file holds exactly n. */
static void test_read_exact(void)
{
    TEST_START("test_read_exact") ;

    size_t const sizes[] = { 0, 13, 5000 } ;
    char pat[5000] ;
    fill_pattern(pat, sizeof pat) ;
    char p[64] ;
    path_in(p, "data") ;

    for (size_t i = 0 ; i < sizeof(sizes)/sizeof(*sizes) ; i++) {
        size_t n = sizes[i] ;
        TEST_ASSERT(file_write(p, pat, n) == 1, "seed write failed") ;
        char rb[5000] ;
        ssize_t r = file_read(p, rb, sizeof rb) ;
        TEST_ASSERT(r == (ssize_t)n, "wrong byte count") ;
        TEST_ASSERT(memcmp(rb, pat, n) == 0, "content mismatch") ;
    }
    unlink(p) ;
    TEST_END() ;
}

/* a file shorter than the buffer yields the partial count, not -1. */
static void test_short_file(void)
{
    TEST_START("test_short_file") ;
    char p[64] ;
    path_in(p, "short") ;
    TEST_ASSERT(file_write(p, "hello", 5) == 1, "seed write failed") ;
    char rb[64] ;
    ssize_t r = file_read(p, rb, sizeof rb) ;
    TEST_ASSERT(r == 5, "short file should give partial count, not -1") ;
    TEST_ASSERT(memcmp(rb, "hello", 5) == 0, "content mismatch") ;
    unlink(p) ;
    TEST_END() ;
}

/* file_read_at composes dir/name. */
static void test_read_at(void)
{
    TEST_START("test_read_at") ;
    char const data[] = "x\0yz" ;   /* embedded NUL, len 4 */
    TEST_ASSERT(file_write_at(tdir, "at", data, 4) == 1, "seed write_at failed") ;
    char rb[16] ;
    ssize_t r = file_read_at(tdir, "at", rb, sizeof rb) ;
    TEST_ASSERT(r == 4, "wrong byte count") ;
    TEST_ASSERT(memcmp(rb, data, 4) == 0, "content mismatch") ;
    char p[64] ; path_in(p, "at") ; unlink(p) ;
    TEST_END() ;
}

/* a missing file yields -1 (open failure propagates through io_readnclose). */
static void test_missing(void)
{
    TEST_START("test_missing") ;
    char rb[16] ;
    errno = 0 ;
    TEST_ASSERT(file_read_at(tdir, "nope", rb, sizeof rb) == -1, "missing file should give -1") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_read / file_read_at test suite ===\n\n") ;

    test_read_exact() ;
    test_short_file() ;
    test_read_at() ;
    test_missing() ;

    rmdir(tdir) ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
