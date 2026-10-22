/*
 * dir_replace_at.c
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

/* Tests for dir_replace_at: the previous @dst is parked in an empty
 * "<tmpdir>/<name>:XXXXXX", @src is renamed onto @dst, the parked copy is
 * destroyed. Every check is made on the effect: content of @dst, absence of
 * @src, listing of @tmpdir (no residue), the old tree put back intact on
 * failure, exact errno. Everything happens under a private mkdtemp directory.
 *
 * Two branches cannot be reached with real failures from an unprivileged test:
 * the reverse rename() that puts the previous @dst back, and dir_destroy() of
 * the parked copy after success. They are reached by interposing rename() and
 * dir_destroy() in this executable. rename() forwards to renameat() unless the
 * call numbered rename_fail_at fails with rename_fail_errno. dir_destroy()
 * fails with EIO when destroy_fail is set, otherwise it removes the tree with
 * nftw() (the library's own dir_destroy is tested in dir_destroy.c). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ftw.h>
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

static char tdir[] = "/tmp/oblibs_dreplace_XXXXXX" ;

/* paths of the current test, set by setup() */
static char tmpd[256], src[256], dst[256] ;

/* rename interposition, see the header comment */
static int rename_calls = 0 ;
static int rename_fail_at = 0 ;
static int rename_fail_errno = 0 ;

int rename(char const *old, char const *new)
{
    rename_calls++ ;
    if (rename_fail_at && rename_calls == rename_fail_at) {
        errno = rename_fail_errno ;
        return -1 ;
    }
    return renameat(AT_FDCWD, old, AT_FDCWD, new) ;
}

/* dir_destroy interposition, see the header comment */
static int destroy_fail = 0 ;

static int rm_one(char const *path, struct stat const *st, int flag, struct FTW *ftw)
{
    (void)st ; (void)flag ; (void)ftw ;
    return remove(path) ;
}

int dir_destroy(char const *dir)
{
    if (destroy_fail) {
        errno = EIO ;
        return 0 ;
    }
    if (nftw(dir, rm_one, 16, FTW_DEPTH | FTW_PHYS) < 0 && errno != ENOENT)
        return 0 ;
    return 1 ;
}

static void reset_interpose(void)
{
    rename_calls = 0 ;
    rename_fail_at = 0 ;
    rename_fail_errno = 0 ;
    destroy_fail = 0 ;
}

static void jp(char *out, size_t max, char const *a, char const *b)
{
    int r = snprintf(out, max, "%s/%s", a, b) ;
    TEST_ASSERT(r > 0 && (size_t)r < max, "path too long") ;
}

static int count_entries_in(char const *dir)
{
    DIR *d = opendir(dir) ;
    TEST_ASSERT(d != NULL, "opendir failed") ;
    int n = 0 ;
    struct dirent *e ;
    while ((e = readdir(d)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            n++ ;
    closedir(d) ;
    return n ;
}

/* the single entry of @dir in @out (full path), its name in @name */
static void only_entry(char const *dir, char *out, size_t max, char *name, size_t nmax)
{
    TEST_ASSERT(count_entries_in(dir) == 1, "expected exactly one entry") ;
    DIR *d = opendir(dir) ;
    TEST_ASSERT(d != NULL, "opendir failed") ;
    struct dirent *e ;
    while ((e = readdir(d)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            break ;
    TEST_ASSERT(e != NULL, "entry vanished") ;
    TEST_ASSERT(strlen(e->d_name) < nmax, "name too long") ;
    strcpy(name, e->d_name) ;
    closedir(d) ;
    jp(out, max, dir, name) ;
}

static int path_absent(char const *path)
{
    struct stat st ;
    int absent = lstat(path, &st) == -1 && errno == ENOENT ;
    errno = 0 ;
    return absent ;
}

static int is_dir(char const *path)
{
    struct stat st ;
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode) ;
}

static void put(char const *path, char const *s)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open for seed failed") ;
    size_t len = strlen(s) ;
    TEST_ASSERT(write(fd, s, len) == (ssize_t)len, "seed write failed") ;
    TEST_ASSERT(close(fd) == 0, "seed close failed") ;
}

static int holds(char const *path, char const *s)
{
    char buf[256] ;
    int fd = open(path, O_RDONLY) ;
    if (fd < 0) return 0 ;
    ssize_t r = read(fd, buf, sizeof buf) ;
    close(fd) ;
    return r == (ssize_t)strlen(s) && !memcmp(buf, s, (size_t)r) ;
}

/* "@dir/@rel" holds exactly @s */
static int at_holds(char const *dir, char const *rel, char const *s)
{
    char p[512] ;
    jp(p, sizeof p, dir, rel) ;
    return holds(p, s) ;
}

static int at_absent(char const *dir, char const *rel)
{
    char p[512] ;
    jp(p, sizeof p, dir, rel) ;
    return path_absent(p) ;
}

/* The OLD tree: f="OLD", g="G", sub/h="H". */
static void make_old(char const *dir)
{
    char p[512] ;
    TEST_ASSERT(mkdir(dir, 0755) == 0, "mkdir old failed") ;
    jp(p, sizeof p, dir, "f") ; put(p, "OLD") ;
    jp(p, sizeof p, dir, "g") ; put(p, "G") ;
    jp(p, sizeof p, dir, "sub") ;
    TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir old/sub failed") ;
    jp(p, sizeof p, dir, "sub/h") ; put(p, "H") ;
}

/* @dir is exactly the OLD tree */
static int is_old(char const *dir)
{
    char p[512] ;
    jp(p, sizeof p, dir, "sub") ;
    return is_dir(dir) && count_entries_in(dir) == 3
        && at_holds(dir, "f", "OLD") && at_holds(dir, "g", "G")
        && count_entries_in(p) == 1 && at_holds(dir, "sub/h", "H") ;
}

/* The NEW tree: f="NEW", n="N". */
static void make_new(char const *dir)
{
    char p[512] ;
    TEST_ASSERT(mkdir(dir, 0755) == 0, "mkdir new failed") ;
    jp(p, sizeof p, dir, "f") ; put(p, "NEW") ;
    jp(p, sizeof p, dir, "n") ; put(p, "N") ;
}

/* @dir is exactly the NEW tree */
static int is_new(char const *dir)
{
    return is_dir(dir) && count_entries_in(dir) == 2
        && at_holds(dir, "f", "NEW") && at_holds(dir, "n", "N") ;
}

/* fresh "tmp", "src", "dst" paths under a fresh case directory; tmp created */
static void setup(char const *casename)
{
    char c[256] ;
    reset_interpose() ;
    jp(c, sizeof c, tdir, casename) ;
    TEST_ASSERT(mkdir(c, 0755) == 0, "mkdir case failed") ;
    jp(tmpd, sizeof tmpd, c, "tmp") ;
    jp(src, sizeof src, c, "src") ;
    jp(dst, sizeof dst, c, "dst") ;
    TEST_ASSERT(mkdir(tmpd, 0755) == 0, "mkdir tmp failed") ;
}

static void teardown(char const *casename)
{
    char c[256] ;
    reset_interpose() ;
    jp(c, sizeof c, tdir, casename) ;
    TEST_ASSERT(dir_destroy(c) == 1, "cleanup failed") ;
}

/* ---------------------------------------------------------------------------
 * success
 * ------------------------------------------------------------------------- */

/* @dst does not exist: the ENOENT of step 2 leads to the single rename, @dst is
 * @src, @src is gone, nothing is left in @tmpdir. */
static void test_dst_absent(void)
{
    TEST_START("test_dst_absent") ;
    setup("absent") ;
    make_new(src) ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 1, "replace failed") ;
    TEST_ASSERT(is_new(dst), "dst is not the new tree") ;
    TEST_ASSERT(path_absent(src), "src still present") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left in tmpdir") ;
    TEST_ASSERT(rename_calls == 2, "expected two renames (step 2, then file_tmp_rename)") ;
    teardown("absent") ;
    TEST_END() ;
}

/* @dst exists: it is replaced by @src, nothing of the old tree survives, @src
 * is gone, nothing is left in @tmpdir. */
static void test_dst_replaced(void)
{
    TEST_START("test_dst_replaced") ;
    setup("replace") ;
    make_old(dst) ;
    make_new(src) ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 1, "replace failed") ;
    TEST_ASSERT(is_new(dst), "dst is not the new tree") ;
    TEST_ASSERT(at_absent(dst, "g") && at_absent(dst, "sub"), "old content left in dst") ;
    TEST_ASSERT(path_absent(src), "src still present") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "old tree left in tmpdir") ;
    teardown("replace") ;
    TEST_END() ;
}

/* @dst given with a trailing '/': the basename is still the last component and
 * the replacement happens. */
static void test_dst_trailing_slash(void)
{
    TEST_START("test_dst_trailing_slash") ;
    setup("slash") ;
    make_old(dst) ;
    make_new(src) ;
    char d[300] ;
    snprintf(d, sizeof d, "%s/", dst) ;
    TEST_ASSERT(dir_replace_at(tmpd, src, d) == 1, "replace failed") ;
    TEST_ASSERT(is_new(dst), "dst is not the new tree") ;
    TEST_ASSERT(path_absent(src), "src still present") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left in tmpdir") ;
    teardown("slash") ;
    TEST_END() ;
}

/* The parked copy cannot be destroyed after the swap (interposed EIO): 1 is
 * still returned, @dst is @src, and the old tree stays in @tmpdir, intact, as
 * "<name>:XXXXXX". */
static void test_destroy_fails_after_success(void)
{
    TEST_START("test_destroy_fails_after_success") ;
    setup("destroyfail") ;
    make_old(dst) ;
    make_new(src) ;
    destroy_fail = 1 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 1, "should still return 1") ;
    destroy_fail = 0 ;
    TEST_ASSERT(is_new(dst), "dst is not the new tree") ;
    TEST_ASSERT(path_absent(src), "src still present") ;
    char p[512], n[256] ;
    only_entry(tmpd, p, sizeof p, n, sizeof n) ;
    TEST_ASSERT(strlen(n) == strlen("dst:XXXXXX") && !strncmp(n, "dst:", 4), "parked name is not <name>:XXXXXX") ;
    TEST_ASSERT(is_old(p), "parked copy is not the old tree") ;
    teardown("destroyfail") ;
    TEST_END() ;
}

/* ---------------------------------------------------------------------------
 * failure
 * ------------------------------------------------------------------------- */

/* @src does not exist, @dst does: 0, ENOENT, the old @dst is put back intact,
 * @tmpdir empty. */
static void test_src_absent(void)
{
    TEST_START("test_src_absent") ;
    setup("srcabsent") ;
    make_old(dst) ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(is_old(dst), "old dst not put back intact") ;
    TEST_ASSERT(path_absent(src), "src appeared") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left in tmpdir") ;
    teardown("srcabsent") ;
    TEST_END() ;
}

/* Neither @src nor @dst exist: 0, ENOENT from the single rename, nothing
 * created anywhere. */
static void test_src_and_dst_absent(void)
{
    TEST_START("test_src_and_dst_absent") ;
    setup("bothabsent") ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(path_absent(dst), "dst appeared") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left in tmpdir") ;
    teardown("bothabsent") ;
    TEST_END() ;
}

/* Step 3 fails with an errno other than ENOENT: @src is the parent of @dst,
 * rename(src, src/dst) gives EINVAL. The old @dst is put back intact and errno
 * is the EINVAL of the move. */
static void test_move_fails_einval(void)
{
    TEST_START("test_move_fails_einval") ;
    setup("ancestor") ;
    char d[300] ;
    jp(d, sizeof d, src, "dst") ;
    TEST_ASSERT(mkdir(src, 0755) == 0, "mkdir src failed") ;
    make_old(d) ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, d) == 0, "replace should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    TEST_ASSERT(is_old(d), "old dst not put back intact") ;
    TEST_ASSERT(count_entries_in(src) == 1, "src modified") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left in tmpdir") ;
    teardown("ancestor") ;
    TEST_END() ;
}

/* The move fails (src absent, ENOENT) and putting the old @dst back fails too
 * (interposed EIO on the third rename): 0, errno is the ENOENT of the move, not
 * the EIO of the reverse rename, @dst does not exist and the old tree stays in
 * @tmpdir, intact. */
static void test_put_back_fails(void)
{
    TEST_START("test_put_back_fails") ;
    setup("putback") ;
    make_old(dst) ;
    rename_fail_at = 3 ;
    rename_fail_errno = EIO ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT (from the move)") ;
    TEST_ASSERT(rename_calls == 3, "the reverse rename was not attempted") ;
    TEST_ASSERT(path_absent(dst), "dst should not exist") ;
    char p[512], n[256] ;
    only_entry(tmpd, p, sizeof p, n, sizeof n) ;
    TEST_ASSERT(!strncmp(n, "dst:", 4), "parked name is not <name>:XXXXXX") ;
    TEST_ASSERT(is_old(p), "parked copy is not the old tree") ;
    teardown("putback") ;
    TEST_END() ;
}

/* @tmpdir does not exist: 0, ENOENT (mkdtemp), nothing moved. */
static void test_tmpdir_absent(void)
{
    TEST_START("test_tmpdir_absent") ;
    setup("tmpabsent") ;
    make_old(dst) ;
    make_new(src) ;
    char t[300] ;
    jp(t, sizeof t, tmpd, "missing") ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(t, src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(is_old(dst), "dst modified") ;
    TEST_ASSERT(is_new(src), "src modified") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left") ;
    TEST_ASSERT(rename_calls == 0, "a rename happened") ;
    teardown("tmpabsent") ;
    TEST_END() ;
}

/* @tmpdir is relative: 0, EINVAL, nothing moved, nothing created. */
static void test_tmpdir_relative(void)
{
    TEST_START("test_tmpdir_relative") ;
    setup("tmprel") ;
    make_old(dst) ;
    make_new(src) ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at("oblibs_relative_tmpdir", src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    TEST_ASSERT(is_old(dst), "dst modified") ;
    TEST_ASSERT(is_new(src), "src modified") ;
    TEST_ASSERT(path_absent("oblibs_relative_tmpdir"), "relative tmpdir created") ;
    TEST_ASSERT(rename_calls == 0, "a rename happened") ;
    teardown("tmprel") ;
    TEST_END() ;
}

/* @dst is a regular file: rename(dst, <empty dir>) gives EISDIR, the empty
 * directory is removed, @dst and @src untouched. */
static void test_dst_not_dir(void)
{
    TEST_START("test_dst_not_dir") ;
    setup("dstfile") ;
    put(dst, "FILE") ;
    make_new(src) ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == EISDIR, "errno should be EISDIR") ;
    TEST_ASSERT(holds(dst, "FILE"), "dst modified") ;
    TEST_ASSERT(is_new(src), "src modified") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "empty directory left in tmpdir") ;
    TEST_ASSERT(rename_calls == 1, "rename after the step 2 failure") ;
    teardown("dstfile") ;
    TEST_END() ;
}

/* Step 2 fails with an errno that is neither ENOENT nor EISDIR: @tmpdir inside
 * @dst, rename(dst, dst/.../x) gives EINVAL. The empty directory is removed
 * (errno restored after that removal), @dst and @src untouched. */
static void test_tmpdir_inside_dst(void)
{
    TEST_START("test_tmpdir_inside_dst") ;
    setup("inside") ;
    make_old(dst) ;
    make_new(src) ;
    char t[300] ;
    jp(t, sizeof t, dst, "sub") ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(t, src, dst) == 0, "replace should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    TEST_ASSERT(is_old(dst), "dst modified or empty directory left in it") ;
    TEST_ASSERT(is_new(src), "src modified") ;
    TEST_ASSERT(rename_calls == 1, "rename after the step 2 failure") ;
    teardown("inside") ;
    TEST_END() ;
}

/* @dst without '/': 0, EINVAL (ob_basename), nothing created, nothing moved. */
static void test_dst_no_slash(void)
{
    TEST_START("test_dst_no_slash") ;
    setup("noslash") ;
    make_new(src) ;
    errno = 0 ;
    TEST_ASSERT(dir_replace_at(tmpd, src, "oblibs_noslash_dst") == 0, "replace should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    TEST_ASSERT(is_new(src), "src modified") ;
    TEST_ASSERT(path_absent("oblibs_noslash_dst"), "dst created") ;
    TEST_ASSERT(count_entries_in(tmpd) == 0, "residue left in tmpdir") ;
    TEST_ASSERT(rename_calls == 0, "a rename happened") ;
    teardown("noslash") ;
    TEST_END() ;
}

int main(void)
{
    umask(022) ;
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== dir_replace_at test suite ===\n\n") ;

    test_dst_absent() ;
    test_dst_replaced() ;
    test_dst_trailing_slash() ;
    test_destroy_fails_after_success() ;
    test_src_absent() ;
    test_src_and_dst_absent() ;
    test_move_fails_einval() ;
    test_put_back_fails() ;
    test_tmpdir_absent() ;
    test_tmpdir_relative() ;
    test_dst_not_dir() ;
    test_tmpdir_inside_dst() ;
    test_dst_no_slash() ;

    TEST_ASSERT(rmdir(tdir) == 0, "test dir not empty at the end") ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
