/*
 * dir_destroy.c
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

/* Tests for dir_destroy (oblibs-native rm -rf, replacing the skalibs rm_rf
 * wrapper). All work happens inside a private mkdtemp sandbox; nothing outside
 * is ever touched. Coverage:
 *   - full recursive removal; symlinks unlinked NOT followed (target survives);
 *   - nonexistent path is success; a single file is removed;
 *   - DIFFERENTIAL vs skalibs rm_rf on identical trees: same success/failure
 *     AND same errno (the core parity requirement);
 *   - the design guarantee: a deep tree is removed under a LOW RLIMIT_NOFILE,
 *     which a naive "keep DIR* open per level" recursion could not do;
 *   - volume (thousands of entries) -> exercises the listing buffer growth;
 *   - awkward inputs: spaces in names, trailing slash, empty string.
 * Permission-based failure cases are skipped when running as root (root
 * bypasses the permission bits, so they cannot fail). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

#include <oblibs/directory.h>

#include <skalibs/djbunix.h>     /* rm_rf */
#include <skalibs/skamisc.h>     /* satmp */
#include <skalibs/stralloc.h>    /* stralloc_free */

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

static char sandbox[] = "/tmp/oblibs_destroy_XXXXXX" ;

static void jp(char *out, size_t n, char const *a, char const *b)
{
    snprintf(out, n, "%s/%s", a, b) ;
}

static void mkfile(char const *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open create") ;
    TEST_ASSERT(write(fd, "x", 1) == 1, "write") ;
    close(fd) ;
}

static int exists(char const *path)
{
    struct stat st ;
    return lstat(path, &st) == 0 ;
}

/* Build a fixed tree at @root, with a symlink pointing at @linktarget (which
 * must never be followed):
 *   root/file1, root/sub/file2, root/sub/deep/file3, root/empty/, root/link
 */
static void make_tree(char const *root, char const *linktarget)
{
    char p[512] ;
    TEST_ASSERT(mkdir(root, 0755) == 0, "mkdir root") ;
    jp(p, sizeof p, root, "file1") ; mkfile(p) ;
    jp(p, sizeof p, root, "sub") ; TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir sub") ;
    { char q[512] ; jp(q, sizeof q, p, "file2") ; mkfile(q) ; }
    { char q[512] ; jp(q, sizeof q, p, "deep") ; TEST_ASSERT(mkdir(q, 0755) == 0, "mkdir deep") ;
      char r[600] ; jp(r, sizeof r, q, "file3") ; mkfile(r) ; }
    jp(p, sizeof p, root, "empty") ; TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir empty") ;
    jp(p, sizeof p, root, "link") ; TEST_ASSERT(symlink(linktarget, p) == 0, "symlink") ;
}

static void test_full_removal(void)
{
    TEST_START("test_full_removal") ;

    char tgt[512], root[512] ;
    jp(tgt, sizeof tgt, sandbox, "target_keepme") ;
    TEST_ASSERT(mkdir(tgt, 0755) == 0, "mkdir target") ;
    { char q[600] ; jp(q, sizeof q, tgt, "precious") ; mkfile(q) ; }

    jp(root, sizeof root, sandbox, "tree_a") ;
    make_tree(root, tgt) ;

    errno = 0 ;
    TEST_ASSERT(dir_destroy(root) == 1, "dir_destroy success") ;
    TEST_ASSERT(!exists(root), "tree removed") ;
    TEST_ASSERT(exists(tgt), "symlink target survived") ;
    { char q[600] ; jp(q, sizeof q, tgt, "precious") ; TEST_ASSERT(exists(q), "target file survived") ; }

    TEST_END() ;
}

static void test_nonexistent_and_file(void)
{
    TEST_START("test_nonexistent_and_file") ;

    char p[512] ;
    jp(p, sizeof p, sandbox, "does_not_exist") ;
    errno = 0 ;
    TEST_ASSERT(dir_destroy(p) == 1, "nonexistent is success") ;

    jp(p, sizeof p, sandbox, "lone_file") ;
    mkfile(p) ;
    TEST_ASSERT(dir_destroy(p) == 1, "single file removed") ;
    TEST_ASSERT(!exists(p), "file gone") ;

    /* empty string: unlink(\"\") -> ENOENT -> treated as success, no crash */
    errno = 0 ;
    TEST_ASSERT(dir_destroy("") == 1, "empty string is success") ;

    TEST_END() ;
}

/* Strict differential in the SUCCESS case. */
static void test_differential_success(void)
{
    TEST_START("test_differential_success") ;

    char tgt[512], a[512], b[512] ;
    jp(tgt, sizeof tgt, sandbox, "diff_target") ;
    TEST_ASSERT(mkdir(tgt, 0755) == 0, "mkdir diff target") ;

    jp(a, sizeof a, sandbox, "diff_a") ;
    jp(b, sizeof b, sandbox, "diff_b") ;
    make_tree(a, tgt) ;
    make_tree(b, tgt) ;

    int rob = dir_destroy(a) ;
    int rsk = rm_rf(b) ;
    stralloc_free(&satmp) ;

    TEST_ASSERT(rob == 1, "oblibs success") ;
    TEST_ASSERT(rsk >= 0, "skalibs success") ;
    TEST_ASSERT(!exists(a) && !exists(b), "both vanished") ;
    TEST_ASSERT(exists(tgt), "shared target survived both") ;

    TEST_END() ;
}

/* Strict differential in a FAILURE case: an unreadable sub-directory makes the
 * removal fail; dir_destroy and rm_rf must agree on failure AND on errno. */
static void test_differential_failure_errno(void)
{
    TEST_START("test_differential_failure_errno") ;

    if (geteuid() == 0) { printf(" SKIP (root)\n") ; return ; }

    char a[512], b[512], suba[600], subb[600] ;
    jp(a, sizeof a, sandbox, "fail_a") ;
    jp(b, sizeof b, sandbox, "fail_b") ;
    TEST_ASSERT(mkdir(a, 0755) == 0 && mkdir(b, 0755) == 0, "mkdir roots") ;
    jp(suba, sizeof suba, a, "locked") ;
    jp(subb, sizeof subb, b, "locked") ;
    TEST_ASSERT(mkdir(suba, 0755) == 0 && mkdir(subb, 0755) == 0, "mkdir subs") ;
    { char q[700] ; jp(q, sizeof q, suba, "x") ; mkfile(q) ; }
    { char q[700] ; jp(q, sizeof q, subb, "x") ; mkfile(q) ; }
    /* remove read+exec on the sub-dirs: opendir() inside will fail with EACCES */
    TEST_ASSERT(chmod(suba, 0000) == 0 && chmod(subb, 0000) == 0, "chmod 000") ;

    errno = 0 ;
    int rob = dir_destroy(a) ;
    int eob = errno ;

    errno = 0 ;
    int rsk = rm_rf(b) ;
    int esk = errno ;
    stralloc_free(&satmp) ;

    TEST_ASSERT(rob == 0, "oblibs reports failure") ;
    TEST_ASSERT(rsk < 0, "skalibs reports failure") ;
    TEST_ASSERT(eob == esk, "same errno as skalibs on failure") ;
    TEST_ASSERT(eob == EACCES, "errno is EACCES") ;

    /* restore perms and clean up the leftovers */
    chmod(suba, 0755) ; chmod(subb, 0755) ;
    TEST_ASSERT(dir_destroy(a) == 1 && dir_destroy(b) == 1, "cleanup") ;

    TEST_END() ;
}

/* The reason for slurp-then-recurse: a deep tree must be removable even when
 * very few file descriptors are available. A recursion that kept a DIR* open
 * at each level would hit EMFILE long before the bottom. */
static void test_deep_under_low_fd_limit(void)
{
    TEST_START("test_deep_under_low_fd_limit") ;

    enum { DEPTH = 200 } ;
    char p[8192] ;
    size_t n = snprintf(p, sizeof p, "%s/deep", sandbox) ;
    TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir deep root") ;
    for (int i = 0 ; i < DEPTH ; i++) {
        n += (size_t)snprintf(p + n, sizeof p - n, "/d") ;
        TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir level") ;
    }
    snprintf(p + n, sizeof p - n, "/leaf") ;
    mkfile(p) ;                       /* also pushes the path well past 256 bytes */

    char root[512] ;
    jp(root, sizeof root, sandbox, "deep") ;

    struct rlimit old ;
    TEST_ASSERT(getrlimit(RLIMIT_NOFILE, &old) == 0, "getrlimit") ;
    struct rlimit low = old ;
    low.rlim_cur = 32 ;               /* << DEPTH: fatal to a per-level-DIR* design */
    TEST_ASSERT(setrlimit(RLIMIT_NOFILE, &low) == 0, "setrlimit low") ;

    errno = 0 ;
    int r = dir_destroy(root) ;

    setrlimit(RLIMIT_NOFILE, &old) ;  /* restore before asserting/printing */

    TEST_ASSERT(r == 1, "deep tree removed under low fd limit") ;
    TEST_ASSERT(!exists(root), "deep tree gone") ;

    TEST_END() ;
}

/* Many entries in a single directory exercise the listing buffer growth. */
static void test_many_entries(void)
{
    TEST_START("test_many_entries") ;

    enum { COUNT = 3000 } ;
    char dir[512] ;
    jp(dir, sizeof dir, sandbox, "many") ;
    TEST_ASSERT(mkdir(dir, 0755) == 0, "mkdir many") ;
    for (int i = 0 ; i < COUNT ; i++) {
        char f[600] ;
        snprintf(f, sizeof f, "%s/f%05d", dir, i) ;
        mkfile(f) ;
    }

    TEST_ASSERT(dir_destroy(dir) == 1, "removed many entries") ;
    TEST_ASSERT(!exists(dir), "many dir gone") ;

    TEST_END() ;
}

/* Awkward names and a trailing slash on the input path. */
static void test_awkward_names(void)
{
    TEST_START("test_awkward_names") ;

    char root[512], p[700] ;
    jp(root, sizeof root, sandbox, "awk") ;
    TEST_ASSERT(mkdir(root, 0755) == 0, "mkdir awk") ;
    jp(p, sizeof p, root, "a file with spaces.txt") ; mkfile(p) ;
    jp(p, sizeof p, root, "sub dir") ; TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir spaced") ;
    { char q[800] ; jp(q, sizeof q, p, "inner one") ; mkfile(q) ; }

    /* destroy via a path WITH a trailing slash */
    char slashed[520] ;
    snprintf(slashed, sizeof slashed, "%s/", root) ;
    TEST_ASSERT(dir_destroy(slashed) == 1, "trailing slash removal") ;
    TEST_ASSERT(!exists(root), "awk tree gone") ;

    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(sandbox) != NULL, "mkdtemp failed") ;

    printf("=== dir_destroy test suite (sandbox %s) ===\n\n", sandbox) ;

    test_full_removal() ;
    test_nonexistent_and_file() ;
    test_differential_success() ;
    test_differential_failure_errno() ;
    test_deep_under_low_fd_limit() ;
    test_many_entries() ;
    test_awkward_names() ;

    TEST_ASSERT(dir_destroy(sandbox) == 1, "sandbox cleanup") ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
