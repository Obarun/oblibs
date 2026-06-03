/*
 * file_write.c
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

/* Differential non-regression test: the oblibs-native file_write_unsafe /
 * file_write_unsafe_g (io_open_mode + io_allwrite) must behave byte-for-byte
 * like skalibs' openwritenclose_unsafe they replaced — same content, same mode,
 * same truncation, and the same unlink-on-failure with errno preserved. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

#include <oblibs/files.h>

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

static char tdir[] = "/tmp/oblibs_fw_XXXXXX" ;

static void fill_pattern(char *buf, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        buf[i] = (char)(i & 0xff) ;
}

/* read the whole file at @path into @buf (max @max bytes); return byte count or -1 */
static ssize_t readfile(char const *path, char *buf, size_t max)
{
    int fd = open(path, O_RDONLY) ;
    if (fd < 0) return -1 ;
    size_t got = 0 ;
    for (;;) {
        ssize_t r = read(fd, buf + got, max - got) ;
        if (r < 0) { if (errno == EINTR) continue ; close(fd) ; return -1 ; }
        if (!r) break ;
        got += (size_t)r ;
        if (got == max) break ;
    }
    close(fd) ;
    return (ssize_t)got ;
}

static void path_in(char *out, char const *name)
{
    size_t dl = strlen(tdir), nl = strlen(name) ;
    memcpy(out, tdir, dl) ;
    out[dl] = '/' ;
    memcpy(out + dl + 1, name, nl + 1) ;
}

/* Case 1+2: byte-for-byte identical content AND identical mode, several sizes
 * (including 0 and a multi-page buffer with embedded NUL bytes). */
static void test_identical(void)
{
    TEST_START("test_identical") ;

    size_t const sizes[] = { 0, 13, 5000 } ;
    char buf[5000] ;
    fill_pattern(buf, sizeof buf) ;

    char pa[64], pb[64] ;
    path_in(pa, "ref_sk") ;     /* written by skalibs openwritenclose_unsafe */
    path_in(pb, "ob") ;         /* written by oblibs file_write_unsafe       */

    for (size_t i = 0 ; i < sizeof(sizes)/sizeof(*sizes) ; i++) {
        size_t n = sizes[i] ;

        TEST_ASSERT(openwritenclose_unsafe(pa, buf, n), "skalibs write failed") ;
        TEST_ASSERT(file_write_unsafe(tdir, "ob", buf, n) == 1, "oblibs write failed") ;

        char ra[5000], rb[5000] ;
        ssize_t la = readfile(pa, ra, sizeof ra) ;
        ssize_t lb = readfile(pb, rb, sizeof rb) ;
        TEST_ASSERT(la == (ssize_t)n && lb == (ssize_t)n, "size mismatch vs skalibs") ;
        TEST_ASSERT(memcmp(ra, rb, n) == 0, "content mismatch vs skalibs") ;

        struct stat sa, sb ;
        TEST_ASSERT(stat(pa, &sa) == 0 && stat(pb, &sb) == 0, "stat failed") ;
        TEST_ASSERT((sa.st_mode & 07777) == (sb.st_mode & 07777), "mode mismatch vs skalibs") ;
    }

    /* file_write_unsafe_g: text content (NUL-terminated), full path form */
    char pc[64], pd[64] ;
    path_in(pc, "ref_sk_g") ;
    path_in(pd, "ob_g") ;
    char const *txt = "hello obarun\nstate line\n" ;
    TEST_ASSERT(openwritenclose_unsafe(pc, txt, strlen(txt)), "skalibs _g write failed") ;
    TEST_ASSERT(file_write_unsafe_g(pd, txt) == 1, "oblibs _g write failed") ;
    char rc[64], rd[64] ;
    ssize_t lc = readfile(pc, rc, sizeof rc) ;
    ssize_t ld = readfile(pd, rd, sizeof rd) ;
    TEST_ASSERT(lc == ld && lc == (ssize_t)strlen(txt), "_g size mismatch") ;
    TEST_ASSERT(memcmp(rc, rd, lc) == 0, "_g content mismatch") ;

    TEST_END() ;
}

/* Case 3: O_TRUNC — a shorter rewrite must leave no tail of the old content. */
static void test_truncate(void)
{
    TEST_START("test_truncate") ;

    char pb[64] ;
    path_in(pb, "trunc") ;

    char longbuf[1024] ;
    fill_pattern(longbuf, sizeof longbuf) ;
    TEST_ASSERT(file_write_unsafe(tdir, "trunc", longbuf, sizeof longbuf) == 1, "long write failed") ;

    char const *shortc = "short" ;
    TEST_ASSERT(file_write_unsafe(tdir, "trunc", shortc, strlen(shortc)) == 1, "short write failed") ;

    char rb[1024] ;
    ssize_t lb = readfile(pb, rb, sizeof rb) ;
    TEST_ASSERT(lb == (ssize_t)strlen(shortc), "truncation left a tail") ;
    TEST_ASSERT(memcmp(rb, shortc, lb) == 0, "truncated content wrong") ;

    TEST_END() ;
}

/* Case 4: write failure — the partial file must be unlinked and errno preserved,
 * exactly like skalibs. RLIMIT_FSIZE makes the write fail deterministically. */
static void test_unlink_on_fail(void)
{
    TEST_START("test_unlink_on_fail") ;

    struct rlimit save ;
    TEST_ASSERT(getrlimit(RLIMIT_FSIZE, &save) == 0, "getrlimit failed") ;

    /* exceeding RLIMIT_FSIZE raises SIGXFSZ; ignore it so write() returns -1/EFBIG */
    signal(SIGXFSZ, SIG_IGN) ;
    struct rlimit lim = { .rlim_cur = 16, .rlim_max = save.rlim_max } ;
    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &lim) == 0, "setrlimit failed") ;

    char buf[4096] ;
    fill_pattern(buf, sizeof buf) ;

    char pb[64], pa[64] ;
    path_in(pb, "ob_fail") ;
    path_in(pa, "sk_fail") ;

    errno = 0 ;
    TEST_ASSERT(file_write_unsafe(tdir, "ob_fail", buf, sizeof buf) == 0, "oblibs write should fail") ;
    TEST_ASSERT(errno == EFBIG, "errno should be EFBIG (preserved)") ;
    struct stat st ;
    errno = 0 ;
    TEST_ASSERT(stat(pb, &st) == -1 && errno == ENOENT, "partial file should be unlinked") ;

    /* skalibs reference: same outcome (returns 0, file absent) */
    TEST_ASSERT(openwritenclose_unsafe(pa, buf, sizeof buf) == 0, "skalibs write should fail") ;
    errno = 0 ;
    TEST_ASSERT(stat(pa, &st) == -1 && errno == ENOENT, "skalibs should also unlink the partial file") ;

    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &save) == 0, "restore setrlimit failed") ;

    TEST_END() ;
}

static void cleanup(void)
{
    char const *names[] = { "ref_sk", "ob", "ref_sk_g", "ob_g", "trunc", "ob_fail", "sk_fail" } ;
    char p[64] ;
    for (size_t i = 0 ; i < sizeof(names)/sizeof(*names) ; i++) {
        path_in(p, names[i]) ;
        unlink(p) ;
    }
    rmdir(tdir) ;
}

int main(void)
{
    /* deterministic mode: 0666 & ~022 = 0644 on both code paths */
    umask(022) ;

    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_write_unsafe differential test suite ===\n\n") ;

    test_identical() ;
    test_truncate() ;
    test_unlink_on_fail() ;

    cleanup() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
