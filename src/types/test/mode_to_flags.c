/*
 * mode_to_flags.c
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
 * Self-contained test for mode_to_flags() and the FLAGS_* file type bits
 * (oblibs/types.h). Every type is checked on a real object where one can be
 * made without privileges (regular file, directory, symlink via lstat,
 * /dev/null, fifo, unix socket, a block device found under /dev), and the
 * whole 4 bit S_IFMT field is swept synthetically so the unknown values and
 * the permission bits are pinned too.
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

/* First block device found directly under /dev (lstat, so a symlink to a block
 * device does not count), copied into @out. Returns 0 if none is visible. */
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

/* The FLAGS_* are seven distinct single bits and FLAGS_ALL is exactly their
 * union: this is the property the old S_IF* masks lacked (S_IFREG|S_IFDIR ==
 * S_IFSOCK). */
static void test_flags_are_bits(void)
{
    TEST_START("test_flags_are_bits") ;
    uint32_t const f[7] = { FLAGS_REG, FLAGS_DIR, FLAGS_LNK, FLAGS_CHR, FLAGS_BLK, FLAGS_FIFO, FLAGS_SOCK } ;
    uint32_t acc = 0 ;
    for (int i = 0 ; i < 7 ; i++) {
        TEST_ASSERT(f[i] && !(f[i] & (f[i] - 1)), "each FLAGS_* is a single bit") ;
        TEST_ASSERT(!(acc & f[i]), "FLAGS_* bits are pairwise disjoint") ;
        acc |= f[i] ;
    }
    TEST_ASSERT(acc == FLAGS_ALL, "FLAGS_ALL is exactly the union of the seven bits") ;
    TEST_END() ;
}

/* Sweep the 16 values of the S_IFMT field: the seven defined types map to their
 * own bit, every other value maps to 0. Each value is also tried with every
 * permission/suid/sgid/sticky bit set, which must not change the result. */
static void test_synthetic_sweep(void)
{
    TEST_START("test_synthetic_sweep") ;
    struct { mode_t type ; uint32_t want ; } const known[] = {
        { S_IFREG, FLAGS_REG }, { S_IFDIR, FLAGS_DIR }, { S_IFLNK, FLAGS_LNK },
        { S_IFCHR, FLAGS_CHR }, { S_IFBLK, FLAGS_BLK }, { S_IFIFO, FLAGS_FIFO },
        { S_IFSOCK, FLAGS_SOCK },
    } ;
    int nknown = 0 ;
    for (unsigned int v = 0 ; v < 16 ; v++) {
        mode_t type = (mode_t)(v << 12) ;
        TEST_ASSERT((type & S_IFMT) == type, "sweep stays inside S_IFMT") ;
        uint32_t want = 0 ;
        for (size_t k = 0 ; k < sizeof known / sizeof *known ; k++)
            if (known[k].type == type) { want = known[k].want ; nknown++ ; }
        TEST_ASSERT(mode_to_flags(type) == want, "type maps to its bit, unknown to 0") ;
        TEST_ASSERT(mode_to_flags(type | 07777) == want, "permission bits 07777 ignored") ;
        TEST_ASSERT(mode_to_flags(type | 0644) == want, "permission bits 0644 ignored") ;
    }
    TEST_ASSERT(nknown == 7, "all seven known types seen in the sweep") ;
    /* explicit anchors for the unknown path */
    TEST_ASSERT(mode_to_flags(0) == 0, "no type -> 0") ;
    TEST_ASSERT(mode_to_flags(S_IFMT) == 0, "S_IFMT all ones -> 0") ;
    TEST_ASSERT(mode_to_flags(0644) == 0, "permission bits only -> 0") ;
    /* the synthetic block device required by the contract */
    TEST_ASSERT(mode_to_flags(S_IFBLK | 0644) == FLAGS_BLK, "S_IFBLK|0644 -> FLAGS_BLK") ;
    TEST_END() ;
}

/* Real objects: lstat each one and convert its st_mode. */
static void test_real_objects(void)
{
    TEST_START("test_real_objects") ;
    char tmpl[] = "/tmp/mode_to_flags_XXXXXX" ;
    char *dir = mkdtemp(tmpl) ;
    TEST_ASSERT(dir != 0, "mkdtemp") ;

    char reg[256], sub[256], lnk[256], fifo[256], sock[256] ;
    snprintf(reg, sizeof reg, "%s/reg", dir) ;
    snprintf(sub, sizeof sub, "%s/dir", dir) ;
    snprintf(lnk, sizeof lnk, "%s/lnk", dir) ;
    snprintf(fifo, sizeof fifo, "%s/fifo", dir) ;
    snprintf(sock, sizeof sock, "%s/sock", dir) ;

    int fd = open(reg, O_CREAT | O_EXCL | O_WRONLY, 0600) ;
    TEST_ASSERT(fd >= 0, "create regular file") ;
    close(fd) ;
    TEST_ASSERT(mkdir(sub, 0700) == 0, "mkdir") ;
    TEST_ASSERT(symlink("reg", lnk) == 0, "symlink") ;
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

    struct { char const *path ; uint32_t want ; char const *what ; } const obj[] = {
        { reg, FLAGS_REG, "regular file -> FLAGS_REG" },
        { sub, FLAGS_DIR, "directory -> FLAGS_DIR" },
        { lnk, FLAGS_LNK, "symlink (lstat) -> FLAGS_LNK" },
        { "/dev/null", FLAGS_CHR, "/dev/null -> FLAGS_CHR" },
        { fifo, FLAGS_FIFO, "fifo -> FLAGS_FIFO" },
        { sock, FLAGS_SOCK, "unix socket -> FLAGS_SOCK" },
    } ;
    for (size_t i = 0 ; i < sizeof obj / sizeof *obj ; i++) {
        struct stat st ;
        TEST_ASSERT(lstat(obj[i].path, &st) == 0, "lstat") ;
        TEST_ASSERT(mode_to_flags(st.st_mode) == obj[i].want, obj[i].what) ;
    }
    /* the symlink seen through stat is its target: a regular file */
    {
        struct stat st ;
        TEST_ASSERT(stat(lnk, &st) == 0, "stat follows the link") ;
        TEST_ASSERT(mode_to_flags(st.st_mode) == FLAGS_REG, "followed link -> FLAGS_REG") ;
    }

    unlink(sock) ; unlink(fifo) ; unlink(lnk) ; rmdir(sub) ; unlink(reg) ; rmdir(dir) ;
    TEST_END() ;
}

/* A real block device if one is visible (lstat only needs the node to exist,
 * no read permission). If none is visible the synthetic S_IFBLK value in
 * test_synthetic_sweep is the only coverage, and this says so. */
static void test_real_blockdev(void)
{
    TEST_START("test_real_blockdev") ;
    char p[300] ;
    if (!find_blockdev(p, sizeof p)) {
        printf(" SKIP (no block device under /dev)\n") ;
        return ;
    }
    struct stat st ;
    TEST_ASSERT(lstat(p, &st) == 0, "lstat block device") ;
    TEST_ASSERT(mode_to_flags(st.st_mode) == FLAGS_BLK, "block device -> FLAGS_BLK") ;
    printf(" [%s]", p) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== mode_to_flags test suite ===\n\n") ;

    test_flags_are_bits() ;
    test_synthetic_sweep() ;
    test_real_objects() ;
    test_real_blockdev() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
