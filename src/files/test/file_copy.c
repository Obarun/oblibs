/*
 * file_copy.c
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

/* Tests for file_copy (oblibs-native replacement for skalibs filecopy_unsafe).
 * All work happens inside a private mkdtemp sandbox. file_copy was proven
 * byte-exact against filecopy_unsafe in a dev differential; here the oracle is
 * self-contained (the copy must reproduce the source content and the requested
 * mode), so the committed test links no skalibs. */

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

#include <oblibs/directory.h>
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

static char sandbox[] = "/tmp/oblibs_filecopy_XXXXXX" ;

static void jp(char *out, size_t n, char const *a, char const *b)
{
    snprintf(out, n, "%s/%s", a, b) ;
}

static void wr(char const *path, char const *content, mode_t mode)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode) ;
    TEST_ASSERT(fd >= 0, "open create") ;
    if (content && *content)
        TEST_ASSERT(write(fd, content, strlen(content)) == (ssize_t)strlen(content), "write") ;
    close(fd) ;
    TEST_ASSERT(chmod(path, mode) == 0, "chmod") ;   /* defeat umask */
}

/* Read up to n-1 bytes of @path into @buf (NUL-terminated). Returns length. */
static size_t slurp(char const *path, char *buf, size_t n)
{
    int fd = open(path, O_RDONLY) ;
    TEST_ASSERT(fd >= 0, "open read") ;
    ssize_t r = read(fd, buf, n - 1) ;
    close(fd) ;
    TEST_ASSERT(r >= 0, "read") ;
    buf[r] = 0 ;
    return (size_t)r ;
}

static void test_basic(void)
{
    TEST_START("test_basic") ;

    char src[512], dst[512], buf[512] ;
    jp(src, sizeof src, sandbox, "src.txt") ;
    jp(dst, sizeof dst, sandbox, "dst.txt") ;
    wr(src, "hello world\n", 0644) ;

    errno = 0 ;
    TEST_ASSERT(file_copy(src, dst, 0640) == 1, "file_copy success") ;

    slurp(dst, buf, sizeof buf) ;
    TEST_ASSERT(!strcmp(buf, "hello world\n"), "content preserved") ;

    struct stat st ;
    TEST_ASSERT(stat(dst, &st) == 0, "stat dst") ;
    TEST_ASSERT((st.st_mode & 07777) == 0640, "mode is the requested one (not the source's)") ;

    TEST_END() ;
}

static void test_empty(void)
{
    TEST_START("test_empty") ;

    char src[512], dst[512], buf[512] ;
    jp(src, sizeof src, sandbox, "empty_src") ;
    jp(dst, sizeof dst, sandbox, "empty_dst") ;
    wr(src, "", 0600) ;

    TEST_ASSERT(file_copy(src, dst, 0600) == 1, "copy empty file") ;
    TEST_ASSERT(slurp(dst, buf, sizeof buf) == 0, "empty content") ;

    struct stat st ;
    TEST_ASSERT(stat(dst, &st) == 0, "stat dst") ;   /* umask(0) in main: requested mode is final */
    TEST_ASSERT((st.st_mode & 07777) == 0600, "empty dst mode is the requested one") ;

    TEST_END() ;
}

/* file_copy truncates an existing dst (O_TRUNC), it does not append/merge. */
static void test_truncates_existing(void)
{
    TEST_START("test_truncates_existing") ;

    char src[512], dst[512], buf[512] ;
    jp(src, sizeof src, sandbox, "short_src") ;
    jp(dst, sizeof dst, sandbox, "long_dst") ;
    wr(src, "abc", 0644) ;
    wr(dst, "PREVIOUS LONGER CONTENT", 0644) ;

    TEST_ASSERT(file_copy(src, dst, 0644) == 1, "copy over existing") ;
    TEST_ASSERT(slurp(dst, buf, sizeof buf) == 3, "dst truncated to src length") ;
    TEST_ASSERT(!strcmp(buf, "abc"), "dst content is src content") ;

    struct stat st ;
    TEST_ASSERT(stat(dst, &st) == 0, "stat dst") ;   /* umask(0) in main: requested mode is final */
    TEST_ASSERT((st.st_mode & 07777) == 0644, "dst mode is the requested one") ;

    TEST_END() ;
}

static void test_missing_src(void)
{
    TEST_START("test_missing_src") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "no_such_file") ;
    jp(dst, sizeof dst, sandbox, "wont_be_created") ;

    errno = 0 ;
    TEST_ASSERT(file_copy(src, dst, 0644) == 0, "missing src is failure") ;
    struct stat st ;
    TEST_ASSERT(lstat(dst, &st) != 0, "dst not created on failure") ;

    TEST_END() ;
}

/* Requested mode is filtered by the umask (io_open_mode passes it straight to
 * open()). Under umask(027), a requested 0666 yields a final 0640. */
static void test_mode_umask(void)
{
    TEST_START("test_mode_umask") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "umask_src") ;
    jp(dst, sizeof dst, sandbox, "umask_dst") ;
    wr(src, "data", 0644) ;

    mode_t saved = umask(027) ;
    TEST_ASSERT(file_copy(src, dst, 0666) == 1, "copy under umask") ;
    umask(saved) ;

    struct stat st ;
    TEST_ASSERT(stat(dst, &st) == 0, "stat dst") ;
    TEST_ASSERT((st.st_mode & 07777) == 0640, "umask(027) masks 0666 down to 0640") ;

    TEST_END() ;
}

/* Multi-buffer transfer: content larger than the 64 KiB internal buffer must be
 * copied byte-for-byte, exercising the io_read/io_allwrite loop across several
 * iterations (a single read cannot satisfy the whole content). */
static void test_large(void)
{
    TEST_START("test_large") ;

    size_t const n = 200000 ;   /* > 3 * 65536 */
    char *content = malloc(n) ;
    TEST_ASSERT(content != NULL, "malloc") ;
    for (size_t i = 0 ; i < n ; i++)
        content[i] = (char)((i * 31 + 7) & 0xff) ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "large_src") ;
    jp(dst, sizeof dst, sandbox, "large_dst") ;

    int fd = open(src, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open large src") ;
    size_t off = 0 ;
    while (off < n) {
        ssize_t w = write(fd, content + off, n - off) ;
        TEST_ASSERT(w > 0, "write large src") ;
        off += (size_t)w ;
    }
    close(fd) ;

    TEST_ASSERT(file_copy(src, dst, 0644) == 1, "copy large file") ;

    struct stat st ;
    TEST_ASSERT(stat(dst, &st) == 0, "stat large dst") ;
    TEST_ASSERT((size_t)st.st_size == n, "large dst size matches src") ;

    char *rb = malloc(n) ;
    TEST_ASSERT(rb != NULL, "malloc rb") ;
    int rfd = open(dst, O_RDONLY) ;
    TEST_ASSERT(rfd >= 0, "open large dst") ;
    off = 0 ;
    while (off < n) {
        ssize_t r = read(rfd, rb + off, n - off) ;
        TEST_ASSERT(r > 0, "read large dst") ;
        off += (size_t)r ;
    }
    close(rfd) ;
    TEST_ASSERT(memcmp(rb, content, n) == 0, "large content byte-exact") ;

    free(rb) ;
    free(content) ;
    TEST_END() ;
}

/* dst open failure: a dst whose parent directory does not exist makes
 * io_open_mode fail. file_copy returns 0 and creates nothing. */
static void test_dst_open_fail(void)
{
    TEST_START("test_dst_open_fail") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "ok_src") ;
    snprintf(dst, sizeof dst, "%s/no_such_dir/dst", sandbox) ;
    wr(src, "payload", 0644) ;

    errno = 0 ;
    TEST_ASSERT(file_copy(src, dst, 0644) == 0, "dst open must fail") ;
    TEST_ASSERT(errno == ENOENT, "errno is ENOENT (missing parent dir)") ;

    struct stat st ;
    TEST_ASSERT(lstat(dst, &st) != 0, "no dst created") ;

    TEST_END() ;
}

/* mid-copy write failure: RLIMIT_FSIZE caps the file size, so a copy larger
 * than the cap fails inside the transfer loop. The header documents that the
 * dst is created/truncated up front, so a mid-copy failure leaves a partial
 * dst -- we assert exactly that: return 0, EFBIG, and a non-empty partial dst. */
static void test_write_fail_partial(void)
{
    TEST_START("test_write_fail_partial") ;

    size_t const n = 200000 ;
    char *content = malloc(n) ;
    TEST_ASSERT(content != NULL, "malloc") ;
    memset(content, 'Z', n) ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "fsz_src") ;
    jp(dst, sizeof dst, sandbox, "fsz_dst") ;

    int fd = open(src, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open fsz src") ;
    size_t off = 0 ;
    while (off < n) {
        ssize_t w = write(fd, content + off, n - off) ;
        TEST_ASSERT(w > 0, "write fsz src") ;
        off += (size_t)w ;
    }
    close(fd) ;

    struct rlimit save ;
    TEST_ASSERT(getrlimit(RLIMIT_FSIZE, &save) == 0, "getrlimit") ;
    signal(SIGXFSZ, SIG_IGN) ;
    struct rlimit lim = { .rlim_cur = 65536, .rlim_max = save.rlim_max } ;
    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &lim) == 0, "setrlimit") ;

    errno = 0 ;
    int rc = file_copy(src, dst, 0644) ;
    int saved_errno = errno ;

    TEST_ASSERT(setrlimit(RLIMIT_FSIZE, &save) == 0, "restore setrlimit") ;

    TEST_ASSERT(rc == 0, "copy must fail on write past RLIMIT_FSIZE") ;
    TEST_ASSERT(saved_errno == EFBIG, "errno is EFBIG") ;

    /* header contract: partial dst remains (created/truncated up front). */
    struct stat st ;
    TEST_ASSERT(stat(dst, &st) == 0, "partial dst exists after failure") ;
    TEST_ASSERT(st.st_size > 0 && (size_t)st.st_size < n, "dst is partial, not full") ;

    free(content) ;
    TEST_END() ;
}

int main(void)
{
    umask(0) ;
    TEST_ASSERT(mkdtemp(sandbox) != NULL, "mkdtemp failed") ;

    printf("=== file_copy test suite (sandbox %s) ===\n\n", sandbox) ;

    test_basic() ;
    test_empty() ;
    test_truncates_existing() ;
    test_missing_src() ;
    test_mode_umask() ;
    test_large() ;
    test_dst_open_fail() ;
    test_write_fail_partial() ;

    TEST_ASSERT(dir_destroy(sandbox) == 1, "sandbox cleanup") ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
