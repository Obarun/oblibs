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
#include <sys/stat.h>
#include <sys/types.h>

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

int main(void)
{
    umask(0) ;
    TEST_ASSERT(mkdtemp(sandbox) != NULL, "mkdtemp failed") ;

    printf("=== file_copy test suite (sandbox %s) ===\n\n", sandbox) ;

    test_basic() ;
    test_empty() ;
    test_truncates_existing() ;
    test_missing_src() ;

    TEST_ASSERT(dir_destroy(sandbox) == 1, "sandbox cleanup") ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
