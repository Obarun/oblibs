/*
 * scan_mode.c
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
 *
 * Self-contained test for scan_mode() (oblibs/files.h). Contract:
 *   - 0, errno EINVAL, if @flags is 0 or holds a bit outside FLAGS_ALL; this is
 *     checked before stat, so the path is never looked at;
 *   - 0, errno left by stat, if stat fails (stat follows symlinks);
 *   - -1, errno EINVAL, if the type of the object is not in @flags;
 *   - 1 if it is.
 * The old get_flags() ANDed S_IF* numbers as if they were bits, so a socket
 * matched S_IFDIR and S_IFREG, and a block device matched S_IFDIR. Those false
 * positives are pinned here as rejections.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <oblibs/files.h>
#include <oblibs/types.h>

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

static uint32_t const ALLF[7] = { FLAGS_REG, FLAGS_DIR, FLAGS_LNK, FLAGS_CHR, FLAGS_BLK, FLAGS_FIFO, FLAGS_SOCK } ;

static char dir[64] ;
static char reg[256], sub[256], lreg[256], ldir[256], ldang[256], fifo[256], sock[256], notdir[256], missing[256] ;

static void setup(void)
{
    char tmpl[] = "/tmp/scan_mode_XXXXXX" ;
    TEST_ASSERT(mkdtemp(tmpl) != 0, "mkdtemp") ;
    snprintf(dir, sizeof dir, "%s", tmpl) ;
    snprintf(reg, sizeof reg, "%s/reg", dir) ;
    snprintf(sub, sizeof sub, "%s/dir", dir) ;
    snprintf(lreg, sizeof lreg, "%s/lreg", dir) ;
    snprintf(ldir, sizeof ldir, "%s/ldir", dir) ;
    snprintf(ldang, sizeof ldang, "%s/ldang", dir) ;
    snprintf(fifo, sizeof fifo, "%s/fifo", dir) ;
    snprintf(sock, sizeof sock, "%s/sock", dir) ;
    snprintf(notdir, sizeof notdir, "%s/reg/x", dir) ;
    snprintf(missing, sizeof missing, "%s/missing", dir) ;

    int fd = open(reg, O_CREAT | O_EXCL | O_WRONLY, 0600) ;
    TEST_ASSERT(fd >= 0, "create regular file") ;
    close(fd) ;
    TEST_ASSERT(mkdir(sub, 0700) == 0, "mkdir") ;
    TEST_ASSERT(symlink("reg", lreg) == 0, "symlink to reg") ;
    TEST_ASSERT(symlink("dir", ldir) == 0, "symlink to dir") ;
    TEST_ASSERT(symlink("nowhere", ldang) == 0, "dangling symlink") ;
    TEST_ASSERT(mkfifo(fifo, 0600) == 0, "mkfifo") ;
    int s = socket(AF_UNIX, SOCK_STREAM, 0) ;
    TEST_ASSERT(s >= 0, "socket") ;
    struct sockaddr_un sa ;
    memset(&sa, 0, sizeof sa) ;
    sa.sun_family = AF_UNIX ;
    TEST_ASSERT(strlen(sock) < sizeof sa.sun_path, "socket path fits") ;
    memcpy(sa.sun_path, sock, strlen(sock) + 1) ;
    TEST_ASSERT(bind(s, (struct sockaddr *)&sa, sizeof sa) == 0, "bind unix socket") ;
    close(s) ;
}

static void teardown(void)
{
    unlink(sock) ; unlink(fifo) ; unlink(ldang) ; unlink(ldir) ; unlink(lreg) ;
    rmdir(sub) ; unlink(reg) ; rmdir(dir) ;
}

static int find_blockdev(char *out, size_t outlen)
{
    DIR *d = opendir("/dev") ;
    if (!d) return 0 ;
    struct dirent *e ;
    int found = 0 ;
    while ((e = readdir(d))) {
        struct stat st ;
        char p[300] ;
        snprintf(p, sizeof p, "/dev/%s", e->d_name) ;
        if (lstat(p, &st) == 0 && S_ISBLK(st.st_mode)) {
            snprintf(out, outlen, "%s", p) ;
            found = 1 ;
            break ;
        }
    }
    closedir(d) ;
    return found ;
}

/* @path must be accepted by exactly its own flag @own, by FLAGS_ALL, and
 * rejected (-1, EINVAL) by every other single flag and by the complement. */
static void check_type(char const *path, uint32_t own, char const *what)
{
    for (int i = 0 ; i < 7 ; i++) {
        errno = 0 ;
        ssize_t r = scan_mode(path, ALLF[i]) ;
        if (ALLF[i] == own) {
            TEST_ASSERT(r == 1, what) ;
        } else {
            TEST_ASSERT(r == -1, what) ;
            TEST_ASSERT(errno == EINVAL, "mismatch sets EINVAL") ;
        }
    }
    errno = 0 ;
    TEST_ASSERT(scan_mode(path, FLAGS_ALL) == 1, "FLAGS_ALL accepts every type") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(path, FLAGS_ALL & ~own) == -1, "complement of own flag rejects") ;
    TEST_ASSERT(errno == EINVAL, "complement mismatch sets EINVAL") ;
}

static void test_each_type(void)
{
    TEST_START("test_each_type") ;
    check_type(reg, FLAGS_REG, "regular file: only FLAGS_REG matches") ;
    check_type(sub, FLAGS_DIR, "directory: only FLAGS_DIR matches") ;
    check_type("/dev/null", FLAGS_CHR, "/dev/null: only FLAGS_CHR matches") ;
    check_type(fifo, FLAGS_FIFO, "fifo: only FLAGS_FIFO matches") ;
    check_type(sock, FLAGS_SOCK, "socket: only FLAGS_SOCK matches") ;
    TEST_END() ;
}

/* The old false positives, named explicitly. */
static void test_old_false_positives(void)
{
    TEST_START("test_old_false_positives") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(sock, FLAGS_DIR) == -1, "socket is not a directory") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(sock, FLAGS_REG) == -1, "socket is not a regular file") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(sock, FLAGS_REG | FLAGS_DIR) == -1, "socket is not REG|DIR") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    TEST_END() ;
}

/* A block device needs a visible node; it cannot be faked for stat(). */
static void test_blockdev(void)
{
    TEST_START("test_blockdev") ;
    char p[300] ;
    if (!find_blockdev(p, sizeof p)) {
        printf(" SKIP (no block device under /dev)\n") ;
        return ;
    }
    check_type(p, FLAGS_BLK, "block device: only FLAGS_BLK matches") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(p, FLAGS_DIR) == -1, "block device is not a directory") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    printf(" [%s]", p) ;
    TEST_END() ;
}

static void test_multiple_flags(void)
{
    TEST_START("test_multiple_flags") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(reg, FLAGS_REG | FLAGS_DIR) == 1, "REG|DIR accepts a regular file") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(sub, FLAGS_REG | FLAGS_DIR) == 1, "REG|DIR accepts a directory") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(fifo, FLAGS_REG | FLAGS_DIR) == -1, "REG|DIR rejects a fifo") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(fifo, FLAGS_FIFO | FLAGS_SOCK) == 1, "FIFO|SOCK accepts a fifo") ;
    TEST_END() ;
}

/* stat() follows the link: the target type decides, never FLAGS_LNK. */
static void test_symlink_followed(void)
{
    TEST_START("test_symlink_followed") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(ldir, FLAGS_DIR) == 1, "link to dir matches FLAGS_DIR") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(ldir, FLAGS_LNK) == -1, "link to dir does not match FLAGS_LNK") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    check_type(lreg, FLAGS_REG, "link to reg: only FLAGS_REG matches") ;
    TEST_END() ;
}

/* stat failure: 0, errno from stat. */
static void test_stat_failure(void)
{
    TEST_START("test_stat_failure") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(ldang, FLAGS_LNK) == 0, "dangling link -> 0") ;
    TEST_ASSERT(errno == ENOENT, "dangling link -> ENOENT") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(ldang, FLAGS_ALL) == 0, "dangling link, FLAGS_ALL -> 0") ;
    TEST_ASSERT(errno == ENOENT, "ENOENT") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(missing, FLAGS_REG) == 0, "missing path -> 0") ;
    TEST_ASSERT(errno == ENOENT, "ENOENT") ;
    errno = 0 ;
    TEST_ASSERT(scan_mode(notdir, FLAGS_REG) == 0, "path through a file -> 0") ;
    TEST_ASSERT(errno == ENOTDIR, "ENOTDIR passed through from stat") ;
    TEST_END() ;
}

/* Invalid flags: 0/EINVAL, checked before stat, so an existing path is not
 * matched and a missing path still yields EINVAL, not ENOENT. */
static void test_invalid_flags(void)
{
    TEST_START("test_invalid_flags") ;
    uint32_t const bad[] = { 0, (uint32_t)S_IFREG, (uint32_t)S_IFDIR, (uint32_t)S_IFSOCK,
                             FLAGS_ALL + (1u << 7), 1u << 7, FLAGS_REG | (1u << 31) } ;
    char const *paths[] = { reg, sub, missing, ldang } ;
    for (size_t i = 0 ; i < sizeof bad / sizeof *bad ; i++) {
        for (size_t j = 0 ; j < sizeof paths / sizeof *paths ; j++) {
            errno = 0 ;
            TEST_ASSERT(scan_mode(paths[j], bad[i]) == 0, "invalid flags -> 0") ;
            TEST_ASSERT(errno == EINVAL, "invalid flags -> EINVAL, before stat") ;
        }
    }
    TEST_END() ;
}

/* The success path does not touch errno (documented). */
static void test_success_keeps_errno(void)
{
    TEST_START("test_success_keeps_errno") ;
    errno = EBADF ;
    TEST_ASSERT(scan_mode(reg, FLAGS_REG) == 1, "regular file matches") ;
    TEST_ASSERT(errno == EBADF, "errno untouched on success") ;
    errno = EBADF ;
    TEST_ASSERT(scan_mode(ldir, FLAGS_REG | FLAGS_DIR) == 1, "link to dir matches REG|DIR") ;
    TEST_ASSERT(errno == EBADF, "errno untouched on success") ;
    TEST_END() ;
}

int main(void)
{
    printf("=== scan_mode test suite ===\n\n") ;

    setup() ;
    test_each_type() ;
    test_old_false_positives() ;
    test_blockdev() ;
    test_multiple_flags() ;
    test_symlink_followed() ;
    test_stat_failure() ;
    test_invalid_flags() ;
    test_success_keeps_errno() ;
    teardown() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
