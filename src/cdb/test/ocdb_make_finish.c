/*
 * ocdb_make_finish.c
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

/* Test for ocdb_make_finish: the maker's hplist strbuf is released on every
 * return path, success or failure.
 *
 * The malloc of the table buffer cannot be made to fail on demand, so malloc()
 * is interposed in this executable. Armed, it fails exactly once (ENOMEM) and
 * records the requested size, so the test proves the failed call is the table
 * buffer allocation and not an unrelated one. Disarmed, it forwards to the real
 * allocator: ASan's when the executable is built with -fsanitize=address (its
 * malloc is a weak symbol, its implementation stays reachable), glibc's
 * otherwise.
 *
 * Every record is added under the same key, so all pairs land in one bucket and
 * the table buffer is exactly 3 * n pairs (2 * n slots for that bucket plus n for
 * the sorted copy). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/cdb.h>
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

/* size of one (hash, position) pair, see cdb-internal.h */
#define HP_SIZE 8

/* malloc interposition, see the header comment */
extern void *__interceptor_malloc(size_t) __attribute__((weak)) ;
extern void *__libc_malloc(size_t) ;

static int malloc_fail_next = 0 ;
static size_t malloc_failed_size = 0 ;

void *malloc(size_t n)
{
    if (malloc_fail_next) {
        malloc_fail_next = 0 ;
        malloc_failed_size = n ;
        errno = ENOMEM ;
        return NULL ;
    }
    return __interceptor_malloc ? __interceptor_malloc(n) : __libc_malloc(n) ;
}

/* start a maker on a fresh temp file and add @n records under one key */
static int prepare(ocdbmaker *c, char *path, unsigned int n)
{
    int fd = mkstemp(path) ;
    TEST_ASSERT(fd >= 0, "mkstemp") ;
    TEST_ASSERT(ocdb_make_start(c, fd), "ocdb_make_start") ;
    for (unsigned int i = 0 ; i < n ; i++) {
        char val[16] ;
        int vlen = snprintf(val, sizeof val, "v%u", i) ;
        TEST_ASSERT(ocdb_make_add(c, "key", 3, val, (uint32_t)vlen), "ocdb_make_add") ;
    }
    return fd ;
}

static int hplist_is_zero(strbuf const *sb)
{
    return !sb->s && !sb->len && !sb->cap && !sb->heap ;
}

/* malloc of the table buffer fails: 0, errno ENOMEM from malloc (not the entry
 * value), the failed call is the 3 * n pairs table buffer, hplist reset to
 * STRBUF_ZERO (its heap block released, LSan reports nothing). */
static void test_finish_malloc_failure(void)
{
    TEST_START("test_finish_malloc_failure") ;
    unsigned int n = 100 ;
    char path[] = "/tmp/oblibs_ocdb_mf_XXXXXX" ;
    /* the maker lives on the heap and is released at the end, so a hplist block
     * left behind becomes unreachable and LSan reports it at exit */
    ocdbmaker *c = malloc(sizeof *c) ;
    TEST_ASSERT(c, "malloc maker") ;
    *c = (ocdbmaker)OCDBMAKER_ZERO ;
    int fd = prepare(c, path, n) ;

    TEST_ASSERT(c->hplist.heap == 1 && c->hplist.s, "hplist not on the heap") ;
    TEST_ASSERT(c->hplist.len == n * HP_SIZE, "hplist does not hold n pairs") ;

    malloc_failed_size = 0 ;
    malloc_fail_next = 1 ;
    errno = EXDEV ;
    int r = ocdb_make_finish(c) ;
    int e = errno ;
    malloc_fail_next = 0 ;

    errno = e ;
    TEST_ASSERT(r == 0, "ocdb_make_finish should fail") ;
    TEST_ASSERT(e == ENOMEM, "errno is not the malloc ENOMEM") ;
    TEST_ASSERT(malloc_failed_size == (size_t)3 * n * HP_SIZE, "the failed malloc is not the table buffer") ;
    TEST_ASSERT(hplist_is_zero(&c->hplist), "hplist not freed on malloc failure") ;

    free(c) ;
    close(fd) ;
    file_tryunlink(path) ;
    TEST_END() ;
}

/* Success: 1, hplist reset to STRBUF_ZERO, and the file is a valid cdb holding
 * every value under the key. */
static void test_finish_success(void)
{
    TEST_START("test_finish_success") ;
    unsigned int n = 100 ;
    char path[] = "/tmp/oblibs_ocdb_ok_XXXXXX" ;
    ocdbmaker *c = malloc(sizeof *c) ;   /* on the heap, see test_finish_malloc_failure */
    TEST_ASSERT(c, "malloc maker") ;
    *c = (ocdbmaker)OCDBMAKER_ZERO ;
    int fd = prepare(c, path, n) ;

    TEST_ASSERT(c->hplist.heap == 1 && c->hplist.s, "hplist not on the heap") ;
    TEST_ASSERT(ocdb_make_finish(c) == 1, "ocdb_make_finish should succeed") ;
    TEST_ASSERT(hplist_is_zero(&c->hplist), "hplist not freed on success") ;
    free(c) ;

    ocdb db = OCDB_ZERO ;
    TEST_ASSERT(ocdb_init_fromfd(&db, fd), "ocdb_init_fromfd") ;
    ocdb_find_state st = OCDB_FIND_STATE_ZERO ;
    unsigned char seen[100] = { 0 } ;
    unsigned int found = 0 ;
    ocdb_data out ;
    int k ;
    while ((k = ocdb_findnext(&db, &out, "key", 3, &st)) == 1) {
        unsigned int idx ;
        char val[16] ;
        TEST_ASSERT(out.len > 1 && out.len < sizeof val, "value length") ;
        memcpy(val, out.s, out.len) ;
        val[out.len] = 0 ;
        TEST_ASSERT(sscanf(val, "v%u", &idx) == 1 && idx < n, "value bytes") ;
        TEST_ASSERT(!seen[idx], "value returned twice") ;
        seen[idx] = 1 ;
        found++ ;
    }
    TEST_ASSERT(k == 0, "findnext clean end") ;
    TEST_ASSERT(found == n, "every value found") ;

    ocdb_data none ;
    TEST_ASSERT(ocdb_find(&db, &none, "nokey", 5) == 0, "absent key -> 0") ;

    ocdb_free(&db) ;
    close(fd) ;
    file_tryunlink(path) ;
    TEST_END() ;
}

/* No record: 1, hplist stays STRBUF_ZERO, and every lookup is absent. */
static void test_finish_empty(void)
{
    TEST_START("test_finish_empty") ;
    char path[] = "/tmp/oblibs_ocdb_em_XXXXXX" ;
    ocdbmaker c = OCDBMAKER_ZERO ;
    int fd = prepare(&c, path, 0) ;

    TEST_ASSERT(ocdb_make_finish(&c) == 1, "ocdb_make_finish should succeed") ;
    TEST_ASSERT(hplist_is_zero(&c.hplist), "hplist not zero") ;

    ocdb db = OCDB_ZERO ;
    TEST_ASSERT(ocdb_init_fromfd(&db, fd), "ocdb_init_fromfd") ;
    ocdb_data none ;
    TEST_ASSERT(ocdb_find(&db, &none, "key", 3) == 0, "absent key -> 0") ;

    ocdb_free(&db) ;
    close(fd) ;
    file_tryunlink(path) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== ocdb_make_finish ===\n") ;
    test_finish_malloc_failure() ;
    test_finish_success() ;
    test_finish_empty() ;
    printf("All tests passed\n") ;
    return 0 ;
}
