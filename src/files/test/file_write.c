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
 * except according to the terms contained in the LICENSE file.
 */

/* Tests for the path writers: file_write (truncate, O_CLOEXEC, mode 0666&~umask),
 * file_write_at (dir/name join), and file_write_atomic (temp+rename: final
 * content correct, no temp residue, previous file untouched on failure) with its
 * file_write_atomic_at branches (empty or missing tmpdir, separator, failed
 * rename, descriptor closed on a short write). Content is checked against the
 * bytes the test asked to write. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

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

static char tdir[] = "/tmp/oblibs_fw_XXXXXX" ;

static void fill_pattern(char *buf, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        buf[i] = (char)(i & 0xff) ;
}

static ssize_t readfile(char const *path, char *buf, size_t max)
{
    int fd = open(path, O_RDONLY) ;
    if (fd < 0) return -1 ;
    size_t got = 0 ;
    for (;;) {
        ssize_t r = read(fd, buf + got, max - got) ;
        if (r < 0) { if (errno == EINTR) continue ; close(fd) ; return -1 ; }
        if (!r || got == max) break ;
        got += (size_t)r ;
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

/* count non-dot entries in tdir */
static int count_entries(void)
{
    DIR *d = opendir(tdir) ;
    TEST_ASSERT(d != NULL, "opendir failed") ;
    int n = 0 ;
    struct dirent *e ;
    while ((e = readdir(d))) if (e->d_name[0] != '.') n++ ;
    closedir(d) ;
    return n ;
}

/* file_write: exact content (incl. embedded NUL), mode 0644, several sizes. */
static void test_write(void)
{
    TEST_START("test_write") ;

    size_t const sizes[] = { 0, 13, 5000 } ;
    char buf[5000] ;
    fill_pattern(buf, sizeof buf) ;
    char p[64] ;
    path_in(p, "w") ;

    for (size_t i = 0 ; i < sizeof(sizes)/sizeof(*sizes) ; i++) {
        size_t n = sizes[i] ;
        TEST_ASSERT(file_write(p, buf, n) == 1, "file_write failed") ;
        char rb[5000] ;
        TEST_ASSERT(readfile(p, rb, sizeof rb) == (ssize_t)n, "size mismatch") ;
        TEST_ASSERT(memcmp(rb, buf, n) == 0, "content mismatch") ;
        struct stat sb ;
        TEST_ASSERT(stat(p, &sb) == 0, "stat failed") ;
        TEST_ASSERT((sb.st_mode & 07777) == 0644, "mode should be 0644") ;
    }
    file_tryunlink(p) ;
    TEST_END() ;
}

/* O_TRUNC: a shorter rewrite leaves no tail of the old content. */
static void test_truncate(void)
{
    TEST_START("test_truncate") ;
    char p[64] ;
    path_in(p, "t") ;
    char longbuf[1024] ;
    fill_pattern(longbuf, sizeof longbuf) ;
    TEST_ASSERT(file_write(p, longbuf, sizeof longbuf) == 1, "long write failed") ;
    TEST_ASSERT(file_write(p, "short", 5) == 1, "short write failed") ;
    char rb[1024] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == 5, "truncation left a tail") ;
    TEST_ASSERT(memcmp(rb, "short", 5) == 0, "truncated content wrong") ;
    file_tryunlink(p) ;
    TEST_END() ;
}

/* file_write_at composes dir/name. */
static void test_write_at(void)
{
    TEST_START("test_write_at") ;
    char const txt[] = "abc\0def" ;   /* embedded NUL, len 7 */
    TEST_ASSERT(file_write_at(tdir, "at", txt, 7) == 1, "file_write_at failed") ;
    char p[64] ;
    path_in(p, "at") ;
    char rb[16] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == 7, "size mismatch") ;
    TEST_ASSERT(memcmp(rb, txt, 7) == 0, "content mismatch") ;
    file_tryunlink(p) ;
    TEST_END() ;
}

/* file_write_atomic: correct final content, no temp residue, final mode 0644.
 * mkstemp creates the temp at 0600; the impl then fchmod()s it to a hardcoded
 * 0644 (fchmod ignores umask), so under a restrictive umask(077) the renamed
 * file must still be 0644 -- this pins the hardcoded chmod, not an umask effect. */
static void test_atomic(void)
{
    TEST_START("test_atomic") ;
    mode_t saved = umask(077) ;   /* would force 0600 if mode came from umask */
    char p[64] ;
    path_in(p, "a") ;
    char buf[3000] ;
    fill_pattern(buf, sizeof buf) ;
    TEST_ASSERT(file_write_atomic(p, buf, sizeof buf) == 1, "atomic write failed") ;
    char rb[3000] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == (ssize_t)sizeof buf, "size mismatch") ;
    TEST_ASSERT(memcmp(rb, buf, sizeof buf) == 0, "content mismatch") ;
    struct stat sb ;
    TEST_ASSERT(stat(p, &sb) == 0, "stat failed") ;
    TEST_ASSERT((sb.st_mode & 07777) == 0644, "atomic final mode must be 0644") ;
    TEST_ASSERT(count_entries() == 1, "temp residue left behind") ;   /* only "a" */
    file_tryunlink(p) ;
    umask(saved) ;
    TEST_END() ;
}

/* atomic write failure: the previous file is untouched and no temp is left.
 * RLIMIT_FSIZE forces the write to fail. */
static void test_atomic_fail_keeps_old(void)
{
    TEST_START("test_atomic_fail_keeps_old") ;
    char p[64] ;
    path_in(p, "a") ;
    TEST_ASSERT(file_write(p, "OLD", 3) == 1, "seed old file failed") ;

    struct rlimit save ;
    TEST_ASSERT(getrlimit(RLIMIT_FSIZE, &save) == 0, "getrlimit failed") ;
    signal(SIGXFSZ, SIG_IGN) ;
    struct rlimit lim = { .rlim_cur = 16, .rlim_max = save.rlim_max } ;
    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &lim) == 0, "setrlimit failed") ;

    char big[4096] ;
    fill_pattern(big, sizeof big) ;
    errno = 0 ;
    TEST_ASSERT(file_write_atomic(p, big, sizeof big) == 0, "atomic write should fail") ;
    TEST_ASSERT(errno == EFBIG, "errno should be EFBIG (preserved)") ;

    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &save) == 0, "restore setrlimit failed") ;

    char rb[16] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == 3, "old file should be intact") ;
    TEST_ASSERT(memcmp(rb, "OLD", 3) == 0, "old content should be intact") ;
    TEST_ASSERT(count_entries() == 1, "temp residue left after failure") ;
    file_tryunlink(p) ;
    TEST_END() ;
}

/* file_write_atomic_at: an empty @tmpdir is rejected with EINVAL before any
 * file is touched. */
static void test_atomic_at_empty_tmpdir(void)
{
    TEST_START("test_atomic_at_empty_tmpdir") ;
    char p[64] ;
    path_in(p, "a") ;
    TEST_ASSERT(file_write(p, "OLD", 3) == 1, "seed old file failed") ;
    errno = 0 ;
    TEST_ASSERT(file_write_atomic_at("", p, "NEW", 3) == 0, "empty tmpdir should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    char rb[16] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == 3 && !memcmp(rb, "OLD", 3), "old file modified") ;
    TEST_ASSERT(count_entries() == 1, "unexpected file created") ;
    file_tryunlink(p) ;
    TEST_END() ;
}

/* file_write_atomic_at: the temporary cannot be created (missing @tmpdir):
 * 0, ENOENT, @file untouched. */
static void test_atomic_at_missing_tmpdir(void)
{
    TEST_START("test_atomic_at_missing_tmpdir") ;
    char p[64], nd[64] ;
    path_in(p, "a") ;
    path_in(nd, "nodir") ;
    TEST_ASSERT(file_write(p, "OLD", 3) == 1, "seed old file failed") ;
    errno = 0 ;
    TEST_ASSERT(file_write_atomic_at(nd, p, "NEW", 3) == 0, "missing tmpdir should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    char rb[16] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == 3 && !memcmp(rb, "OLD", 3), "old file modified") ;
    TEST_ASSERT(count_entries() == 1, "unexpected file created") ;
    file_tryunlink(p) ;
    TEST_END() ;
}

/* file_write_atomic_at: the temporary is "<tmpdir>/<basename>.XXXXXX", with
 * the '/' added only when @tmpdir lacks one. A 200-byte tmpdir component and a
 * 60-byte basename make the two readings distinguishable: without the separator
 * "<200>" "<60>" ".XXXXXX" is a single 267-byte component, over NAME_MAX. Both
 * spellings of @tmpdir (with and without the trailing '/') must succeed and leave
 * no temporary behind. */
static void test_atomic_at_separator(void)
{
    TEST_START("test_atomic_at_separator") ;
    char sub[300], subsl[301], f[400], name[61] ;
    memset(name, 'n', 60) ;
    name[60] = 0 ;
    int r = snprintf(sub, sizeof sub, "%s/%0200d", tdir, 0) ;
    TEST_ASSERT(r > 0 && (size_t)r < sizeof sub, "path too long") ;
    snprintf(subsl, sizeof subsl, "%s/", sub) ;
    snprintf(f, sizeof f, "%s/%s", tdir, name) ;
    TEST_ASSERT(mkdir(sub, 0755) == 0, "mkdir failed") ;

    char const *dirs[] = { sub, subsl } ;
    for (size_t i = 0 ; i < 2 ; i++) {
        char content[8] = "round-0" ;
        content[6] = (char)('0' + i) ;
        TEST_ASSERT(file_write_atomic_at(dirs[i], f, content, 7) == 1, "atomic_at write failed") ;
        char rb[16] ;
        TEST_ASSERT(readfile(f, rb, sizeof rb) == 7 && !memcmp(rb, content, 7), "content mismatch") ;
        DIR *d = opendir(sub) ;
        TEST_ASSERT(d != NULL, "opendir failed") ;
        struct dirent *e ;
        int n = 0 ;
        while ((e = readdir(d))) if (e->d_name[0] != '.') n++ ;
        closedir(d) ;
        TEST_ASSERT(n == 0, "temporary left in tmpdir") ;
        TEST_ASSERT(count_entries() == 2, "expected only the subdir and the file") ;
    }
    file_tryunlink(f) ;
    TEST_ASSERT(rmdir(sub) == 0, "rmdir failed") ;
    TEST_END() ;
}

/* file_write_atomic_at: the final rename fails (@file is a non-empty
 * directory): 0, EISDIR, the directory untouched, no temporary left. */
static void test_atomic_at_commit_fail(void)
{
    TEST_START("test_atomic_at_commit_fail") ;
    char dst[64], inner[80] ;
    path_in(dst, "d") ;
    TEST_ASSERT(mkdir(dst, 0755) == 0, "mkdir failed") ;
    snprintf(inner, sizeof inner, "%s/keep", dst) ;
    TEST_ASSERT(file_write(inner, "KEEP", 4) == 1, "seed failed") ;
    errno = 0 ;
    TEST_ASSERT(file_write_atomic_at(tdir, dst, "NEW", 3) == 0, "rename onto a dir should fail") ;
    TEST_ASSERT(errno == EISDIR, "errno should be EISDIR") ;
    char rb[16] ;
    TEST_ASSERT(readfile(inner, rb, sizeof rb) == 4 && !memcmp(rb, "KEEP", 4), "directory modified") ;
    TEST_ASSERT(count_entries() == 1, "temporary left behind") ;
    file_tryunlink(inner) ;
    TEST_ASSERT(rmdir(dst) == 0, "rmdir failed") ;
    TEST_END() ;
}

/* file_write_atomic_at: on a short write the temporary's descriptor is closed
 * (no leak). The descriptor number is the lowest free one, predicted before. */
static void test_atomic_at_write_fail_closes_fd(void)
{
    TEST_START("test_atomic_at_write_fail_closes_fd") ;
    char p[64] ;
    path_in(p, "a") ;

    int nfd = open("/dev/null", O_RDONLY) ;
    TEST_ASSERT(nfd >= 0, "open /dev/null failed") ;
    close(nfd) ;

    struct rlimit save ;
    TEST_ASSERT(getrlimit(RLIMIT_FSIZE, &save) == 0, "getrlimit failed") ;
    signal(SIGXFSZ, SIG_IGN) ;
    struct rlimit lim = { .rlim_cur = 16, .rlim_max = save.rlim_max } ;
    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &lim) == 0, "setrlimit failed") ;
    char big[4096] ;
    fill_pattern(big, sizeof big) ;
    errno = 0 ;
    int r = file_write_atomic_at(tdir, p, big, sizeof big) ;
    int e = errno ;
    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &save) == 0, "restore setrlimit failed") ;
    errno = e ;
    TEST_ASSERT(r == 0, "atomic_at write should fail") ;
    TEST_ASSERT(errno == EFBIG, "errno should be EFBIG") ;
    errno = 0 ;
    TEST_ASSERT(fcntl(nfd, F_GETFD) == -1 && errno == EBADF, "temporary fd leaked") ;
    errno = 0 ;
    TEST_ASSERT(count_entries() == 0, "temporary left behind") ;
    TEST_END() ;
}

int main(void)
{
    umask(022) ;   /* deterministic mode: 0666 & ~022 = 0644 */
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_write / file_write_at / file_write_atomic test suite ===\n\n") ;

    test_write() ;
    test_truncate() ;
    test_write_at() ;
    test_atomic() ;
    test_atomic_fail_keeps_old() ;
    test_atomic_at_empty_tmpdir() ;
    test_atomic_at_missing_tmpdir() ;
    test_atomic_at_separator() ;
    test_atomic_at_commit_fail() ;
    test_atomic_at_write_fail_closes_fd() ;

    rmdir(tdir) ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
