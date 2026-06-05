/*
 * ostream.c
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

/* Tests for the ostream buffered writer. A tiny 16-byte backing buffer forces
 * the interesting paths deterministically: plain buffering, auto-flush when a
 * put does not fit, and the write-through path for a chunk >= capacity. The
 * stream targets a temp file, which is read back and compared byte-for-byte so
 * ordering across buffered/flushed/write-through writes is verified. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>

#include <oblibs/ostream.h>

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

static char tpath[] = "/tmp/oblibs_ostream_XXXXXX" ;

static void test_mixed_paths(void)
{
    TEST_START("test_mixed_paths") ;

    int fd = mkstemp(tpath) ;
    TEST_ASSERT(fd >= 0, "mkstemp failed") ;

    ostream o ;
    char storage[16] ;                 /* capacity = 15 */
    TEST_ASSERT(ostream_init(&o, fd, storage, sizeof storage), "ostream_init failed") ;

    char big[40] ;
    memset(big, 'C', sizeof big) ;

    TEST_ASSERT(ostream_puts(&o, "AAAA"), "puts AAAA") ;            /* buffered */
    TEST_ASSERT(ostream_puts(&o, "BBBBBBBBBBBB"), "puts B*12") ;    /* flush AAAA, buffer B*12 */
    TEST_ASSERT(ostream_put(&o, big, sizeof big), "put big") ;      /* flush B*12, write-through 40 C */

    struct iovec v[2] = { { "DD", 2 }, { "EEE", 3 } } ;
    TEST_ASSERT(ostream_putv(&o, v, 2), "putv") ;                   /* buffer DDEEE */
    TEST_ASSERT(ostream_putflush(&o, "FF", 2), "putflush FF") ;     /* buffer FF, flush DDEEEFF */
    TEST_ASSERT(ostream_flush(&o), "final flush") ;                 /* nothing left */

    /* Expected: AAAA + B*12 + C*40 + DDEEE + FF */
    char expected[4 + 12 + 40 + 5 + 2] ;
    size_t n = 0 ;
    memcpy(expected + n, "AAAA", 4) ; n += 4 ;
    memset(expected + n, 'B', 12) ; n += 12 ;
    memset(expected + n, 'C', 40) ; n += 40 ;
    memcpy(expected + n, "DDEEE", 5) ; n += 5 ;
    memcpy(expected + n, "FF", 2) ; n += 2 ;

    TEST_ASSERT(lseek(fd, 0, SEEK_SET) == 0, "lseek failed") ;
    char got[sizeof expected + 8] ;
    ssize_t r = read(fd, got, sizeof got) ;
    TEST_ASSERT(r == (ssize_t)n, "file size mismatch") ;
    TEST_ASSERT(memcmp(got, expected, n) == 0, "file content mismatch") ;

    close(fd) ;
    unlink(tpath) ;
    TEST_END() ;
}

/* Empty puts are no-ops that still succeed. */
static void test_empty_put(void)
{
    TEST_START("test_empty_put") ;

    ostream o ;
    char storage[16] ;
    TEST_ASSERT(ostream_init(&o, 1, storage, sizeof storage), "init") ;
    TEST_ASSERT(ostream_put(&o, "", 0), "empty put should succeed") ;
    TEST_ASSERT(ostream_flush(&o), "flush of empty should succeed") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== ostream test suite ===\n\n") ;

    test_mixed_paths() ;
    test_empty_put() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
