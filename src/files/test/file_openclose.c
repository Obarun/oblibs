/*
 * file_openclose.c
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

/* Non-regression test for the oblibs-native file_openclose_readonly
 * (io_open + io_allread + close), which dropped skalibs' fd_close. Covers the
 * full-read semantics (file larger/smaller/equal to the buffer, empty file),
 * errno preservation on success (the fd_close -> close replacement), and the
 * error paths (missing file, real read error). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
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

static char tdir[] = "/tmp/oblibs_oc_XXXXXX" ;
static char path[64] ;

static void fill_pattern(char *buf, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        buf[i] = (char)(i & 0xff) ;
}

static void writefile(size_t len)
{
    char buf[5000] ;
    fill_pattern(buf, len) ;
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

/* Reads of file vs buffer for the various size relations; content must match
 * the file's bytes and errno must be preserved across a successful call. */
static void test_sizes(void)
{
    TEST_START("test_sizes") ;

    char pat[5000] ;
    fill_pattern(pat, sizeof pat) ;
    char buf[128] ;

    /* file > buffer: only buffsize bytes land in buf */
    writefile(100) ;
    errno = EXDEV ;   /* sentinel: a successful call must restore it */
    int r = file_openclose_readonly(buf, path, 10) ;
    TEST_ASSERT(r >= 0, "file>buffer should succeed") ;
    TEST_ASSERT(memcmp(buf, pat, 10) == 0, "first buffsize bytes mismatch") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    /* file < buffer: EOF reached, whole file read (this is success, not error) */
    writefile(20) ;
    memset(buf, 0x55, sizeof buf) ;
    errno = EXDEV ;
    r = file_openclose_readonly(buf, path, sizeof buf) ;
    TEST_ASSERT(r >= 0, "file<buffer should succeed (EOF is not an error)") ;
    TEST_ASSERT(memcmp(buf, pat, 20) == 0, "short file content mismatch") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    /* empty file */
    writefile(0) ;
    errno = EXDEV ;
    r = file_openclose_readonly(buf, path, sizeof buf) ;
    TEST_ASSERT(r >= 0, "empty file should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    /* exact fit: file size == buffsize */
    writefile(64) ;
    memset(buf, 0x55, sizeof buf) ;
    r = file_openclose_readonly(buf, path, 64) ;
    TEST_ASSERT(r >= 0, "exact-fit should succeed") ;
    TEST_ASSERT(memcmp(buf, pat, 64) == 0, "exact-fit content mismatch") ;

    TEST_END() ;
}

/* Error paths: missing file -> ENOENT ; a directory target -> EISDIR on read,
 * errno must survive the internal close() (i.e. not be clobbered). */
static void test_errors(void)
{
    TEST_START("test_errors") ;

    char buf[64] ;
    char missing[64] ;
    size_t dl = strlen(tdir) ;
    memcpy(missing, tdir, dl) ;
    memcpy(missing + dl, "/nope", sizeof("/nope")) ;

    errno = 0 ;
    int r = file_openclose_readonly(buf, missing, sizeof buf) ;
    TEST_ASSERT(r == -1, "missing file should fail") ;
    TEST_ASSERT(errno == ENOENT, "missing file errno should be ENOENT") ;

    /* a directory: open(O_RDONLY) succeeds, read() returns EISDIR */
    errno = 0 ;
    r = file_openclose_readonly(buf, tdir, sizeof buf) ;
    TEST_ASSERT(r == -1, "reading a directory should fail") ;
    TEST_ASSERT(errno == EISDIR, "read error errno should be EISDIR (not clobbered by close)") ;

    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;
    size_t dl = strlen(tdir) ;
    memcpy(path, tdir, dl) ;
    memcpy(path + dl, "/data", sizeof("/data")) ;

    printf("=== file_openclose_readonly test suite ===\n\n") ;

    test_sizes() ;
    test_errors() ;

    unlink(path) ;
    rmdir(tdir) ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
