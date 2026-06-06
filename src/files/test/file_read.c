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

/* Differential non-regression test: the oblibs-native file_readputsa
 * (io_open + io_allread + strbuf) must behave like the former skalibs
 * openreadfileclose path -- same bytes appended into the caller's buffer,
 * same NUL-terminated/len-- convention, same append-at-len and error
 * semantics. The caller buffer is now a strbuf; the skalibs openreadfileclose
 * reference (a stralloc) is kept as the ground-truth oracle. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/files.h>
#include <oblibs/strbuf.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

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

static void writefile(char const *path, char const *buf, size_t len)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open for write failed") ;
    size_t put = 0 ;
    while (put < len) {
        ssize_t w = write(fd, buf + put, len - put) ;
        TEST_ASSERT(w >= 0, "write failed") ;
        put += (size_t)w ;
    }
    close(fd) ;
}

/* Case 1: content == ground truth, and parity with skalibs openreadfileclose. */
static void test_content(void)
{
    TEST_START("test_content") ;

    size_t const sizes[] = { 0, 13, 5000 } ;
    char pat[5000] ;
    fill_pattern(pat, sizeof pat) ;

    char path[64] ;
    path_in(path, "data") ;

    for (size_t i = 0 ; i < sizeof(sizes)/sizeof(*sizes) ; i++) {
        size_t n = sizes[i] ;
        writefile(path, pat, n) ;

        strbuf res = STRBUF_ZERO ;
        TEST_ASSERT(file_read(&res, tdir, "data") == 1, "file_readputsa failed") ;
        TEST_ASSERT(res.len == n, "wrong length") ;
        TEST_ASSERT(memcmp(res.s, pat, n) == 0, "content != ground truth") ;
        TEST_ASSERT(res.s[res.len] == 0, "result not NUL-terminated past len") ;

        /* skalibs reference: same flow the old implementation used */
        stralloc ref = STRALLOC_ZERO ;
        TEST_ASSERT(openreadfileclose(path, &ref, 0), "skalibs read failed") ;
        TEST_ASSERT(stralloc_0(&ref), "stralloc_0 failed") ;
        ref.len-- ;
        TEST_ASSERT(res.len == ref.len, "length differs from skalibs") ;
        TEST_ASSERT(memcmp(res.s, ref.s, res.len) == 0, "content differs from skalibs") ;

        strbuf_free(&res) ;
        stralloc_free(&ref) ;
    }

    TEST_END() ;
}

/* Case 2: append into a non-empty buffer preserves the existing prefix. */
static void test_append(void)
{
    TEST_START("test_append") ;

    char const *content = "file body" ;
    char path[64] ;
    path_in(path, "data") ;
    writefile(path, content, strlen(content)) ;

    strbuf sa = STRBUF_ZERO ;
    char const *prefix = "PREFIX:" ;
    TEST_ASSERT(strbuf_cats(&sa, prefix), "seed prefix failed") ;
    size_t base = sa.len ;

    TEST_ASSERT(file_read(&sa, tdir, "data") == 1, "file_readputsa failed") ;
    TEST_ASSERT(sa.len == base + strlen(content), "wrong appended length") ;
    TEST_ASSERT(memcmp(sa.s, prefix, base) == 0, "prefix clobbered") ;
    TEST_ASSERT(memcmp(sa.s + base, content, strlen(content)) == 0, "appended content wrong") ;

    strbuf_free(&sa) ;
    TEST_END() ;
}

/* Case 3: missing file -> 0, ENOENT, buffer restored. */
static void test_missing(void)
{
    TEST_START("test_missing") ;

    /* wasnull branch: fresh buffer must end up freed (s == NULL) */
    strbuf fresh = STRBUF_ZERO ;
    errno = 0 ;
    TEST_ASSERT(file_read(&fresh, tdir, "nope") == 0, "should fail on missing file") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(fresh.s == NULL, "fresh buffer should be freed") ;

    /* non-null branch: len must be restored to base, prefix preserved */
    strbuf sa = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_cats(&sa, "KEEP"), "seed failed") ;
    size_t base = sa.len ;
    errno = 0 ;
    TEST_ASSERT(file_read(&sa, tdir, "nope") == 0, "should fail on missing file") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(sa.len == base, "len should be restored to base") ;
    TEST_ASSERT(memcmp(sa.s, "KEEP", base) == 0, "prefix should be preserved") ;
    strbuf_free(&sa) ;

    TEST_END() ;
}

/* Case 4: non-regular target (a directory) -> 0, ENOENT (scan_mode S_IFREG fails). */
static void test_not_regular(void)
{
    TEST_START("test_not_regular") ;

    strbuf sa = STRBUF_ZERO ;
    errno = 0 ;
    /* tdir itself is a directory, read it via its parent */
    TEST_ASSERT(file_read(&sa, "/", "tmp") == 0, "should fail on a directory") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    strbuf_free(&sa) ;

    TEST_END() ;
}

static void cleanup(void)
{
    char p[64] ;
    path_in(p, "data") ;
    unlink(p) ;
    rmdir(tdir) ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_readputsa differential test suite ===\n\n") ;

    test_content() ;
    test_append() ;
    test_missing() ;
    test_not_regular() ;

    cleanup() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
