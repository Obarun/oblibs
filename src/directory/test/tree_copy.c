/*
 * tree_copy.c
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

/* Tests for tree_copy (oblibs-native recursive hierarchy copy, replacing the
 * skalibs hiercopy). All work happens inside a private mkdtemp sandbox.
 * tree_copy was proven manifest-exact against hiercopy in a dev differential;
 * here the oracle is self-contained: the copy must reproduce the SOURCE tree
 * node-for-node (type, mode, content, symlink target), so the committed test
 * links no skalibs. Coverage: regular files (several modes), nested dirs, an
 * empty dir, a fifo, a symlink (copied NOT followed) and a dangling symlink; a
 * pre-existing destination directory; copying a single file; idempotent re-copy
 * onto a fully populated destination (every node type already present, including
 * an existing symlink that must be replaced) and the fatal path when that
 * replacement cannot proceed. Device/socket nodes need privileges (mknod) and
 * are not exercised here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/directory.h>

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

static char sandbox[] = "/tmp/oblibs_treecopy_XXXXXX" ;

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

/* Build the source tree at @root (@root must not exist yet). */
static void make_src(char const *root)
{
    char p[1024] ;
    TEST_ASSERT(mkdir(root, 0755) == 0, "mkdir root") ;
    jp(p, sizeof p, root, "file_a") ;        wr(p, "hello world", 0644) ;
    jp(p, sizeof p, root, "exec.sh") ;       wr(p, "#!/bin/sh\necho hi\n", 0755) ;
    jp(p, sizeof p, root, "empty") ;         wr(p, "", 0600) ;
    jp(p, sizeof p, root, "sub") ;           TEST_ASSERT(mkdir(p, 0750) == 0, "mkdir sub") ;
    { char q[1100] ; jp(q, sizeof q, p, "nested") ; wr(q, "nested content", 0644) ;
      char d[1100] ; jp(d, sizeof d, p, "deep") ; TEST_ASSERT(mkdir(d, 0700) == 0, "mkdir deep") ;
      char l[1200] ; jp(l, sizeof l, d, "leaf") ; wr(l, "leaf", 0644) ; }
    jp(p, sizeof p, root, "emptydir") ;      TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir emptydir") ;
    jp(p, sizeof p, root, "afifo") ;         TEST_ASSERT(mkfifo(p, 0644) == 0, "mkfifo") ;
    jp(p, sizeof p, root, "link_ok") ;       TEST_ASSERT(symlink("file_a", p) == 0, "symlink ok") ;
    jp(p, sizeof p, root, "link_dangling") ; TEST_ASSERT(symlink("nowhere", p) == 0, "symlink dangling") ;
}

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

/* Assert that the node at @b reproduces the node at @a exactly: same type, same
 * permission bits, and same payload (content for files, target for symlinks,
 * recursively for directories). This is the self-contained oracle. */
static void same_node(char const *a, char const *b)
{
    struct stat sa, sb ;
    TEST_ASSERT(lstat(a, &sa) == 0, "lstat a") ;
    TEST_ASSERT(lstat(b, &sb) == 0, "lstat b (node missing in copy)") ;
    TEST_ASSERT((sa.st_mode & S_IFMT) == (sb.st_mode & S_IFMT), "same node type") ;
    TEST_ASSERT((sa.st_mode & 07777) == (sb.st_mode & 07777), "same permission bits") ;

    if (S_ISREG(sa.st_mode)) {
        char ca[8192], cb[8192] ;
        size_t la = slurp(a, ca, sizeof ca), lb = slurp(b, cb, sizeof cb) ;
        TEST_ASSERT(la == lb && !memcmp(ca, cb, la), "same file content") ;
    }
    else if (S_ISLNK(sa.st_mode)) {
        char ta[4096], tb[4096] ;
        ssize_t la = readlink(a, ta, sizeof ta - 1), lb = readlink(b, tb, sizeof tb - 1) ;
        TEST_ASSERT(la >= 0 && lb >= 0 && la == lb && !memcmp(ta, tb, la), "same symlink target") ;
    }
    else if (S_ISDIR(sa.st_mode)) {
        struct dirent **list ;
        int n = scandir(a, &list, 0, alphasort) ;
        TEST_ASSERT(n >= 0, "scandir a") ;
        for (int i = 0 ; i < n ; i++) {
            char const *nm = list[i]->d_name ;
            if (!(nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0)))) {
                char ca[1100], cb[1100] ;
                jp(ca, sizeof ca, a, nm) ; jp(cb, sizeof cb, b, nm) ;
                same_node(ca, cb) ;
            }
            free(list[i]) ;
        }
        free(list) ;
    }
}

static void test_full_copy(void)
{
    TEST_START("test_full_copy") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "src") ;
    jp(dst, sizeof dst, sandbox, "dst") ;
    make_src(src) ;

    errno = 0 ;
    TEST_ASSERT(tree_copy(src, dst) == 1, "tree_copy success") ;
    same_node(src, dst) ;

    /* spot checks on the special nodes */
    char p[512] ; struct stat st ;
    jp(p, sizeof p, dst, "afifo") ;   TEST_ASSERT(lstat(p, &st) == 0 && S_ISFIFO(st.st_mode), "fifo copied as fifo") ;
    jp(p, sizeof p, dst, "link_ok") ; TEST_ASSERT(lstat(p, &st) == 0 && S_ISLNK(st.st_mode), "symlink copied, not followed") ;

    TEST_END() ;
}

/* tree_copy tolerates a destination directory that already exists. */
static void test_existing_dst(void)
{
    TEST_START("test_existing_dst") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "src2") ;
    jp(dst, sizeof dst, sandbox, "dst2") ;
    make_src(src) ;
    TEST_ASSERT(mkdir(dst, 0755) == 0, "pre-create dst") ;

    TEST_ASSERT(tree_copy(src, dst) == 1, "tree_copy into existing dir") ;
    same_node(src, dst) ;

    TEST_END() ;
}

/* The top-level source may be a single regular file, not a directory. */
static void test_single_file(void)
{
    TEST_START("test_single_file") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "lone.txt") ;
    jp(dst, sizeof dst, sandbox, "lone_copy.txt") ;
    wr(src, "single", 0640) ;

    TEST_ASSERT(tree_copy(src, dst) == 1, "tree_copy single file") ;
    same_node(src, dst) ;

    TEST_END() ;
}

/* Re-copying onto a populated destination must be idempotent for the SYMLINK
 * case: an existing destination symlink (the regression — hiercopy succeeded,
 * the first tree_copy port did not, breaking 66 snapshot on a service with an
 * [Environment] section) must be replaced, not error out, like existing files
 * (O_TRUNC) and dirs (EEXIST tolerated). The tree here mirrors that real shape
 * (dir + versioned subdir + a "version" symlink) and contains NO fifo: only the
 * symlink branch is meant to become idempotent, so an unrelated node type must
 * not muddy the assertion. */
static void test_idempotent_recopy(void)
{
    TEST_START("test_idempotent_recopy") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "src3") ;
    jp(dst, sizeof dst, sandbox, "dst3") ;

    char p[1100] ;
    TEST_ASSERT(mkdir(src, 0755) == 0, "mkdir src3") ;
    jp(p, sizeof p, src, "0.1") ; TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir 0.1") ;
    { char f[1200] ; jp(f, sizeof f, p, "data") ; wr(f, "v1", 0644) ; }
    jp(p, sizeof p, src, "version") ; TEST_ASSERT(symlink("0.1", p) == 0, "src version symlink") ;

    TEST_ASSERT(tree_copy(src, dst) == 1, "first copy") ;
    /* Second copy onto the now-populated destination: the "version" symlink
     * already exists. It must still succeed. */
    errno = 0 ;
    TEST_ASSERT(tree_copy(src, dst) == 1, "recopy onto populated dst (symlink idempotence)") ;
    same_node(src, dst) ;

    /* Repoint the source symlink, recopy, and assert the destination symlink was
     * actually replaced (not left stale at its old target). */
    jp(p, sizeof p, src, "version") ;
    TEST_ASSERT(unlink(p) == 0, "drop src symlink") ;
    TEST_ASSERT(symlink("0.2", p) == 0, "repoint src symlink") ;
    TEST_ASSERT(tree_copy(src, dst) == 1, "recopy after repoint") ;
    char dlp[1100], tgt[256] ;
    jp(dlp, sizeof dlp, dst, "version") ;
    ssize_t tl = readlink(dlp, tgt, sizeof tgt - 1) ;
    TEST_ASSERT(tl > 0, "readlink dst symlink") ;
    tgt[tl] = 0 ;
    TEST_ASSERT(!strcmp(tgt, "0.2"), "dst symlink replaced, not stale") ;

    TEST_END() ;
}

/* A genuine error in the symlink branch must stay fatal: if the existing
 * destination symlink cannot be removed (its parent dir is not writable),
 * tree_copy fails rather than silently succeeding. */
static void test_symlink_replace_fatal(void)
{
    TEST_START("test_symlink_replace_fatal") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "src4") ;
    jp(dst, sizeof dst, sandbox, "dst4") ;

    /* src: a directory holding a single symlink. */
    TEST_ASSERT(mkdir(src, 0755) == 0, "mkdir src4") ;
    char p[1100] ;
    jp(p, sizeof p, src, "lnk") ; TEST_ASSERT(symlink("whatever", p) == 0, "src symlink") ;

    /* dst: same shape, already populated, then lock the parent dir read-only so
     * unlink() of the existing symlink fails with EACCES. */
    TEST_ASSERT(mkdir(dst, 0755) == 0, "mkdir dst4") ;
    jp(p, sizeof p, dst, "lnk") ; TEST_ASSERT(symlink("stale", p) == 0, "dst symlink") ;
    TEST_ASSERT(chmod(dst, 0500) == 0, "lock dst dir") ;

    errno = 0 ;
    int r = tree_copy(src, dst) ;
    TEST_ASSERT(chmod(dst, 0755) == 0, "unlock dst dir") ;   /* restore for cleanup */
    TEST_ASSERT(r == 0, "unremovable existing symlink is a fatal error") ;

    TEST_END() ;
}

static void test_missing_src(void)
{
    TEST_START("test_missing_src") ;

    char src[512], dst[512] ;
    jp(src, sizeof src, sandbox, "no_such_tree") ;
    jp(dst, sizeof dst, sandbox, "no_dst") ;

    errno = 0 ;
    TEST_ASSERT(tree_copy(src, dst) == 0, "missing src is failure") ;

    TEST_END() ;
}

int main(void)
{
    umask(0) ;
    TEST_ASSERT(mkdtemp(sandbox) != NULL, "mkdtemp failed") ;

    printf("=== tree_copy test suite (sandbox %s) ===\n\n", sandbox) ;

    test_full_copy() ;
    test_existing_dst() ;
    test_single_file() ;
    test_idempotent_recopy() ;
    test_symlink_replace_fatal() ;
    test_missing_src() ;

    TEST_ASSERT(dir_destroy(sandbox) == 1, "sandbox cleanup") ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
