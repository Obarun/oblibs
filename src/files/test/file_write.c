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

/* Tests for the path writers: file_write (truncate, O_CLOEXEC, mode 0666&~umask),
 * file_write_at (dir/name join), and file_write_atomic (temp+rename: final
 * content correct, no temp residue, previous file untouched on failure). Content
 * is checked against the bytes the test asked to write. */

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

/* file_write_atomic: correct final content, no temp residue. */
static void test_atomic(void)
{
    TEST_START("test_atomic") ;
    char p[64] ;
    path_in(p, "a") ;
    char buf[3000] ;
    fill_pattern(buf, sizeof buf) ;
    TEST_ASSERT(file_write_atomic(p, buf, sizeof buf) == 1, "atomic write failed") ;
    char rb[3000] ;
    TEST_ASSERT(readfile(p, rb, sizeof rb) == (ssize_t)sizeof buf, "size mismatch") ;
    TEST_ASSERT(memcmp(rb, buf, sizeof buf) == 0, "content mismatch") ;
    TEST_ASSERT(count_entries() == 1, "temp residue left behind") ;   /* only "a" */
    file_tryunlink(p) ;
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

    rmdir(tdir) ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
