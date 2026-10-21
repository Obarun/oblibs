/*
 * sync_fd.c
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

/* Test for sync_fd: fsync with EINTR retry, errno restored to its entry value on
 * success, the fsync errno kept on failure (EBADF, EINVAL for a pipe).
 *
 * fsync() is interposed in this executable to count the calls and to inject
 * EINTR, which a real fsync on a test filesystem cannot be made to return. The
 * interposer forwards to the real syscall once the injected EINTRs are spent. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

#include <oblibs/fd.h>

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

static char tdir[] = "/tmp/oblibs_sync_XXXXXX" ;

static int fsync_calls = 0 ;
static int fsync_eintr = 0 ;

int fsync(int fd)
{
    fsync_calls++ ;
    if (fsync_eintr > 0) {
        fsync_eintr-- ;
        errno = EINTR ;
        return -1 ;
    }
    return (int)syscall(SYS_fsync, fd) ;
}

static int open_file(void)
{
    char p[64] ;
    snprintf(p, sizeof p, "%s/f", tdir) ;
    int fd = open(p, O_RDWR | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open failed") ;
    TEST_ASSERT(write(fd, "data", 4) == 4, "write failed") ;
    return fd ;
}

static void drop_file(int fd)
{
    char p[64] ;
    snprintf(p, sizeof p, "%s/f", tdir) ;
    close(fd) ;
    unlink(p) ;
}

/* Regular file and directory: 0, one fsync, errno sentinel untouched. */
static void test_sync_regular(void)
{
    TEST_START("test_sync_regular") ;
    int fd = open_file() ;
    fsync_calls = 0 ;
    errno = EXDEV ;
    TEST_ASSERT(sync_fd(fd) == 0, "sync_fd on a file should succeed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;
    TEST_ASSERT(fsync_calls == 1, "fsync not called exactly once") ;
    drop_file(fd) ;

    int dfd = open(tdir, O_RDONLY | O_DIRECTORY) ;
    TEST_ASSERT(dfd >= 0, "open dir failed") ;
    errno = ENOTTY ;
    TEST_ASSERT(sync_fd(dfd) == 0, "sync_fd on a directory should succeed") ;
    TEST_ASSERT(errno == ENOTTY, "errno not preserved on success") ;
    close(dfd) ;
    TEST_END() ;
}

/* EINTR is retried until fsync completes; errno is restored even though the
 * interrupted attempts set it to EINTR. */
static void test_sync_eintr_retry(void)
{
    TEST_START("test_sync_eintr_retry") ;
    int fd = open_file() ;
    fsync_calls = 0 ;
    fsync_eintr = 3 ;
    errno = EXDEV ;
    int r = sync_fd(fd) ;
    TEST_ASSERT(r == 0, "sync_fd should succeed after EINTR") ;
    TEST_ASSERT(errno == EXDEV, "errno not restored after EINTR retries") ;
    TEST_ASSERT(fsync_calls == 4, "expected 3 interrupted attempts + 1") ;
    TEST_ASSERT(fsync_eintr == 0, "injected EINTR not consumed") ;
    drop_file(fd) ;
    TEST_END() ;
}

/* EINTR then a real error: the retry stops on the real error and reports it. */
static void test_sync_eintr_then_error(void)
{
    TEST_START("test_sync_eintr_then_error") ;
    int fd = open_file() ;
    drop_file(fd) ;               /* fd is now closed */
    fsync_calls = 0 ;
    fsync_eintr = 2 ;
    errno = 0 ;
    TEST_ASSERT(sync_fd(fd) == -1, "sync_fd on a closed fd should fail") ;
    TEST_ASSERT(errno == EBADF, "errno should be EBADF") ;
    TEST_ASSERT(fsync_calls == 3, "expected 2 interrupted attempts + 1") ;
    TEST_END() ;
}

/* Closed descriptor and -1: EBADF, a single attempt. */
static void test_sync_badfd(void)
{
    TEST_START("test_sync_badfd") ;
    int fd = open_file() ;
    drop_file(fd) ;
    int const bad[] = { fd, -1 } ;
    for (size_t i = 0 ; i < 2 ; i++) {
        fsync_calls = 0 ;
        errno = 0 ;
        TEST_ASSERT(sync_fd(bad[i]) == -1, "sync_fd should fail") ;
        TEST_ASSERT(errno == EBADF, "errno should be EBADF") ;
        TEST_ASSERT(fsync_calls == 1, "a non-EINTR error must not be retried") ;
    }
    TEST_END() ;
}

/* A pipe (and a character device) do not support synchronization: EINVAL. */
static void test_sync_pipe(void)
{
    TEST_START("test_sync_pipe") ;
    int p[2] ;
    TEST_ASSERT(pipe(p) == 0, "pipe failed") ;
    for (size_t i = 0 ; i < 2 ; i++) {
        errno = 0 ;
        TEST_ASSERT(sync_fd(p[i]) == -1, "sync_fd on a pipe should fail") ;
        TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    }
    close(p[0]) ;
    close(p[1]) ;

    int nul = open("/dev/null", O_WRONLY) ;
    TEST_ASSERT(nul >= 0, "open /dev/null failed") ;
    errno = 0 ;
    TEST_ASSERT(sync_fd(nul) == -1, "sync_fd on /dev/null should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    close(nul) ;
    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== sync_fd test suite ===\n\n") ;

    test_sync_regular() ;
    test_sync_eintr_retry() ;
    test_sync_eintr_then_error() ;
    test_sync_badfd() ;
    test_sync_pipe() ;

    TEST_ASSERT(rmdir(tdir) == 0, "test dir not empty at the end") ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
