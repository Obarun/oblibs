/*
 * ocdb_find.c
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

/*
 * Functional test for the ocdb reader. Three strategies:
 *   - ROUND-TRIP: build the shared dataset with ocdb_make_*, then ocdb_find every
 *     key and check the value matches; ocdb_traverse_next must yield every record
 *     in insertion order.
 *   - MULTI-VALUE: several values under one key are all retrievable via
 *     ocdb_findnext (set equality, order is hash-table dependent).
 *   - ROBUSTNESS: a truncated database makes ocdb_find return -1 (bounds-checked),
 *     never a crash nor a false hit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/cdb.h>
#include <oblibs/types.h>
#include <oblibs/files.h>

#include "cdb_dataset.h"

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

/* build a cdb from @pairs into a fresh temp file; return its fd, path in @path */
static int build(char *path, cdb_pair const *pairs, unsigned int n)
{
    int fd = mkstemp(path) ;
    if (fd < 0) return -1 ;
    ocdbmaker c = OCDBMAKER_ZERO ;
    if (!ocdb_make_start(&c, fd)) { close(fd) ; return -1 ; }
    for (unsigned int i = 0 ; i < n ; i++)
        if (!ocdb_make_add(&c, (char const *)pairs[i].key, pairs[i].klen,
                               (char const *)pairs[i].val, pairs[i].vlen)) { close(fd) ; return -1 ; }
    if (!ocdb_make_finish(&c)) { close(fd) ; return -1 ; }
    return fd ;
}

static void test_roundtrip_and_traverse(void)
{
    TEST_START("test_roundtrip_and_traverse") ;

    cdb_pair *ds = malloc(sizeof(cdb_pair) * CDB_DATASET_N) ;
    TEST_ASSERT(ds, "malloc") ;
    cdb_dataset_fill(ds) ;

    char path[] = "/tmp/oblibs_ocdb_rt_XXXXXX" ;
    int fd = build(path, ds, CDB_DATASET_N) ;
    TEST_ASSERT(fd >= 0, "build") ;

    ocdb c = OCDB_ZERO ;
    TEST_ASSERT(ocdb_init_fromfd(&c, fd), "ocdb_init_fromfd") ;

    /* every key resolves to its value */
    for (unsigned int i = 0 ; i < CDB_DATASET_N ; i++) {
        ocdb_data out ;
        int r = ocdb_find(&c, &out, (char const *)ds[i].key, ds[i].klen) ;
        TEST_ASSERT(r == 1, "key found") ;
        TEST_ASSERT(out.len == ds[i].vlen, "value length") ;
        TEST_ASSERT(memcmp(out.s, ds[i].val, ds[i].vlen) == 0, "value bytes") ;
    }

    /* a key that does not exist is reported absent */
    {
        ocdb_data out ;
        char nokey[5] = { (char)0xde, (char)0xad, (char)0xbe, (char)0xef, (char)0x00 } ;
        int r = ocdb_find(&c, &out, nokey, sizeof nokey) ;
        TEST_ASSERT(r == 0, "absent key -> 0") ;
    }

    /* traversal yields every record, in insertion order */
    {
        uint32_t pos = OCDB_TRAVERSE_INIT ;
        ocdb_data k, d ;
        unsigned int i = 0 ;
        int r ;
        while ((r = ocdb_traverse_next(&c, &k, &d, &pos)) == 1) {
            TEST_ASSERT(i < CDB_DATASET_N, "traverse count <= N") ;
            TEST_ASSERT(k.len == ds[i].klen && memcmp(k.s, ds[i].key, k.len) == 0, "traverse key") ;
            TEST_ASSERT(d.len == ds[i].vlen && memcmp(d.s, ds[i].val, d.len) == 0, "traverse data") ;
            i++ ;
        }
        TEST_ASSERT(r == 0, "traverse clean end") ;
        TEST_ASSERT(i == CDB_DATASET_N, "traverse count == N") ;
    }

    ocdb_free(&c) ;
    close(fd) ;
    file_tryunlink(path) ;
    free(ds) ;
    TEST_END() ;
}

static void test_multivalue(void)
{
    TEST_START("test_multivalue") ;

    /* three values under the same key, plus one other key */
    cdb_pair pairs[4] ;
    memset(pairs, 0, sizeof pairs) ;
    for (int i = 0 ; i < 3 ; i++) {
        memcpy(pairs[i].key, "dup", 3) ; pairs[i].klen = 3 ;
        pairs[i].val[0] = (unsigned char)('A' + i) ; pairs[i].vlen = 1 ;
    }
    memcpy(pairs[3].key, "solo", 4) ; pairs[3].klen = 4 ;
    memcpy(pairs[3].val, "x", 1) ; pairs[3].vlen = 1 ;

    char path[] = "/tmp/oblibs_ocdb_mv_XXXXXX" ;
    int fd = build(path, pairs, 4) ;
    TEST_ASSERT(fd >= 0, "build") ;

    ocdb c = OCDB_ZERO ;
    TEST_ASSERT(ocdb_init_fromfd(&c, fd), "init") ;

    /* collect all values under "dup": must be exactly {A, B, C} */
    int seen[3] = { 0, 0, 0 } ;
    int count = 0 ;
    ocdb_find_state st = OCDB_FIND_STATE_ZERO ;
    ocdb_data out ;
    int r ;
    while ((r = ocdb_findnext(&c, &out, "dup", 3, &st)) == 1) {
        TEST_ASSERT(out.len == 1, "dup value len") ;
        int idx = out.s[0] - 'A' ;
        TEST_ASSERT(idx >= 0 && idx < 3, "dup value in range") ;
        TEST_ASSERT(!seen[idx], "no duplicate value returned") ;
        seen[idx] = 1 ;
        count++ ;
    }
    TEST_ASSERT(r == 0, "findnext clean end") ;
    TEST_ASSERT(count == 3 && seen[0] && seen[1] && seen[2], "all three values seen") ;

    /* the other key is unaffected */
    TEST_ASSERT(ocdb_find(&c, &out, "solo", 4) == 1 && out.len == 1 && out.s[0] == 'x', "solo") ;

    ocdb_free(&c) ;
    close(fd) ;
    file_tryunlink(path) ;
    TEST_END() ;
}

static void test_robustness_truncated(void)
{
    TEST_START("test_robustness_truncated") ;

    cdb_pair *ds = malloc(sizeof(cdb_pair) * CDB_DATASET_N) ;
    TEST_ASSERT(ds, "malloc") ;
    cdb_dataset_fill(ds) ;

    char path[] = "/tmp/oblibs_ocdb_trunc_XXXXXX" ;
    int fd = build(path, ds, CDB_DATASET_N) ;
    TEST_ASSERT(fd >= 0, "build") ;

    /* end-of-data = first hash table position = header[0..3] (little-endian) */
    char hb[4] ;
    TEST_ASSERT(lseek(fd, 0, SEEK_SET) == 0 && read(fd, hb, 4) == 4, "read header[0]") ;
    uint32_t eod ;
    u32_unpack(hb, &eod) ;

    /* cut off all hash tables: the header still points at them, beyond EOF */
    TEST_ASSERT(ftruncate(fd, (off_t)eod) == 0, "truncate to eod") ;

    ocdb c = OCDB_ZERO ;
    TEST_ASSERT(ocdb_init_fromfd(&c, fd), "init truncated") ;

    /* a key that exists has a non-empty bucket whose table is now gone -> -1 */
    ocdb_data out ;
    errno = 0 ;
    int r = ocdb_find(&c, &out, (char const *)ds[0].key, ds[0].klen) ;
    TEST_ASSERT(r == -1, "truncated -> -1 (bounds-checked)") ;
    TEST_ASSERT(errno == EPROTO, "errno EPROTO on malformed") ;

    ocdb_free(&c) ;
    close(fd) ;
    file_tryunlink(path) ;
    free(ds) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== ocdb_find test suite ===\n\n") ;
    test_roundtrip_and_traverse() ;
    test_multivalue() ;
    test_robustness_truncated() ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
