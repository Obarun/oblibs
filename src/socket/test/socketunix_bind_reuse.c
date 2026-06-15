/*
 * socketunix_bind_reuse.c
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

/* Behavioural non-regression test for socketunix_bind_reuse (oblibs-native
 * ipc_bind_reuse). This function has filesystem side effects rather than a byte
 * output, so the proof is behavioural/errno-exact: the socket node and its
 * "<path>.lock" sidecar must appear, the lock must be held (a concurrent binder
 * gets EBUSY), releasing it must allow a rebind over the stale node, and the
 * argument guards must report EINVAL / EPROTO. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <oblibs/socket.h>
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

static char tdir[] = "/tmp/oblibs_sockreuse_XXXXXX" ;
static char spath[128] ;
static char lpath[128] ;

/* Happy path: creates the socket node and the .lock, holds the lock, then a
 * second binder is refused with EBUSY while fdlock is open; releasing it lets a
 * fresh binder rebind over the stale node. */
static void test_bind_lock_rebind(void)
{
    TEST_START("test_bind_lock_rebind") ;

    int s = socketunix_create(O_CLOEXEC) ;
    TEST_ASSERT(s >= 0, "socket create failed") ;

    int fdlock = -1 ;
    TEST_ASSERT(socketunix_bind_reuse(s, spath, &fdlock) == 0, "bind_reuse failed") ;
    TEST_ASSERT(fdlock >= 0, "fdlock not set") ;
    /* Core contract: the lock must be close-on-exec so it releases at exec. */
    TEST_ASSERT(fcntl(fdlock, F_GETFD) & FD_CLOEXEC, "lock fd must be O_CLOEXEC") ;

    struct stat st ;
    TEST_ASSERT(lstat(spath, &st) == 0, "socket node missing") ;
    TEST_ASSERT(S_ISSOCK(st.st_mode), "node is not a socket") ;
    TEST_ASSERT(lstat(lpath, &st) == 0, ".lock sidecar missing") ;

    /* The lock must really be held: an independent descriptor contends. */
    int probe = open(lpath, O_WRONLY) ;
    TEST_ASSERT(probe >= 0, "open lock probe failed") ;
    TEST_ASSERT(lock_fd(probe, 1, 1) == 0, "lock should be held by fdlock") ;
    close(probe) ;

    /* A second binder, while fdlock is still open, must fail with EBUSY. */
    int s2 = socketunix_create(O_CLOEXEC) ;
    TEST_ASSERT(s2 >= 0, "socket create 2 failed") ;
    int fdlock2 = -1 ;
    errno = 0 ;
    TEST_ASSERT(socketunix_bind_reuse(s2, spath, &fdlock2) == -1, "second bind should fail") ;
    TEST_ASSERT(errno == EBUSY, "contended bind must report EBUSY") ;
    close(s2) ;

    /* Release the lock; a fresh binder now rebinds over the stale socket node. */
    close(fdlock) ;
    int s3 = socketunix_create(O_CLOEXEC) ;
    TEST_ASSERT(s3 >= 0, "socket create 3 failed") ;
    int fdlock3 = -1 ;
    TEST_ASSERT(socketunix_bind_reuse(s3, spath, &fdlock3) == 0, "rebind after release failed") ;
    TEST_ASSERT(fdlock3 >= 0, "fdlock3 not set") ;
    TEST_ASSERT(lstat(spath, &st) == 0 && S_ISSOCK(st.st_mode), "socket node missing after rebind") ;

    close(fdlock3) ;
    close(s) ;
    close(s3) ;
    TEST_END() ;
}

/* Error paths beyond the argument guards: the lock file cannot be created (its
 * parent directory is missing) -> failure with no socket bound; and a bind that
 * fails (non-socket fd) -> failure with the lock released, not leaked. */
static void test_error_paths(void)
{
    TEST_START("test_error_paths") ;

    char missing[128], bpath[128], blockf[128], reg[128] ;

    /* (a) lock open fails: "<path>.lock" sits under a non-existent directory. */
    snprintf(missing, sizeof missing, "%s/nope/s", tdir) ;
    int s = socketunix_create(O_CLOEXEC) ;
    TEST_ASSERT(s >= 0, "socket create failed") ;
    int fdlock = -1 ;
    errno = 0 ;
    TEST_ASSERT(socketunix_bind_reuse(s, missing, &fdlock) == -1, "lock-open failure not reported") ;
    TEST_ASSERT(errno == ENOENT, "missing-dir lock must report ENOENT") ;
    TEST_ASSERT(fdlock == -1, "fdlock must stay untouched on failure") ;
    struct stat st ;
    TEST_ASSERT(lstat(missing, &st) != 0, "no socket node must be left behind") ;
    close(s) ;

    /* (b) bind fails: pass a regular-file fd as the "socket". The lock file is
     * created and taken, bind() then fails ENOTSOCK; the lock must be released. */
    snprintf(bpath, sizeof bpath, "%s/b", tdir) ;
    snprintf(blockf, sizeof blockf, "%s/b.lock", tdir) ;
    snprintf(reg, sizeof reg, "%s/reg", tdir) ;
    int notsock = open(reg, O_CREAT | O_WRONLY, 0600) ;
    TEST_ASSERT(notsock >= 0, "open reg file failed") ;
    fdlock = -1 ;
    errno = 0 ;
    TEST_ASSERT(socketunix_bind_reuse(notsock, bpath, &fdlock) == -1, "bind failure not reported") ;
    TEST_ASSERT(errno == ENOTSOCK, "non-socket bind must report ENOTSOCK") ;
    TEST_ASSERT(fdlock == -1, "fdlock must stay untouched on failure") ;

    /* The lock must have been released: an independent descriptor can take it. */
    int probe = open(blockf, O_WRONLY) ;
    TEST_ASSERT(probe >= 0, "open b.lock failed (it should have been created)") ;
    TEST_ASSERT(lock_fd(probe, 1, 1) == 1, "lock must be released after bind failure") ;
    close(probe) ;

    close(notsock) ;
    unlink(reg) ;
    unlink(blockf) ;
    TEST_END() ;
}

/* Argument guards: NULL -> EINVAL, over-long path -> EPROTO. */
static void test_guards(void)
{
    TEST_START("test_guards") ;

    int s = socketunix_create(O_CLOEXEC) ;
    TEST_ASSERT(s >= 0, "socket create failed") ;
    int fdlock = -1 ;

    errno = 0 ;
    TEST_ASSERT(socketunix_bind_reuse(s, NULL, &fdlock) == -1, "NULL path should fail") ;
    TEST_ASSERT(errno == EINVAL, "NULL path must report EINVAL") ;

    errno = 0 ;
    TEST_ASSERT(socketunix_bind_reuse(s, "/x", NULL) == -1, "NULL fdlock should fail") ;
    TEST_ASSERT(errno == EINVAL, "NULL fdlock must report EINVAL") ;

    char toolong[SOCKETUNIX_PATH_MAX + 4] ;
    memset(toolong, 'a', sizeof(toolong) - 1) ;
    toolong[sizeof(toolong) - 1] = 0 ;
    errno = 0 ;
    TEST_ASSERT(socketunix_bind_reuse(s, toolong, &fdlock) == -1, "long path should fail") ;
    TEST_ASSERT(errno == EPROTO, "long path must report EPROTO") ;

    close(s) ;
    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    size_t dl = strlen(tdir) ;
    memcpy(spath, tdir, dl) ;
    memcpy(spath + dl, "/s", 3) ;
    memcpy(lpath, spath, strlen(spath)) ;
    memcpy(lpath + strlen(spath), ".lock", 6) ;

    printf("=== socketunix_bind_reuse test suite ===\n\n") ;

    test_bind_lock_rebind() ;
    test_error_paths() ;
    test_guards() ;

    unlink(spath) ;
    unlink(lpath) ;
    rmdir(tdir) ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
