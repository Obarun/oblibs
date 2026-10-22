/*
 * file_tmp_rename.c
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

/* Tests for file_tmp_rename: rename(tmp, dst) for a temporary without a
 * descriptor. On success @dst is what @tmp was (regular file, symbolic link or
 * directory) and @tmp is gone. On failure 0 is returned with the rename errno,
 * a file or symbolic link @tmp is unlinked (the link itself, never its target),
 * a directory @tmp stays in place, @dst is untouched. Every check is made on the
 * effect: lstat of the paths, file content, link target, directory listing,
 * exact errno. Everything happens under a private mkdtemp directory. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
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

static char tdir[] = "/tmp/oblibs_ftren_XXXXXX" ;

static void path_in(char *out, size_t max, char const *name)
{
    int r = snprintf(out, max, "%s/%s", tdir, name) ;
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

static int count_entries(void)
{
    return count_entries_in(tdir) ;
}

static int path_absent(char const *path)
{
    struct stat st ;
    int absent = lstat(path, &st) == -1 && errno == ENOENT ;
    errno = 0 ;
    return absent ;
}

/* lstat type of @path is @type (S_IFREG, S_IFLNK, S_IFDIR) */
static int is_type(char const *path, mode_t type)
{
    struct stat st ;
    return lstat(path, &st) == 0 && (st.st_mode & S_IFMT) == type ;
}

static void put(char const *path, char const *s)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "open for seed failed") ;
    size_t len = strlen(s) ;
    TEST_ASSERT(write(fd, s, len) == (ssize_t)len, "seed write failed") ;
    TEST_ASSERT(close(fd) == 0, "seed close failed") ;
}

/* exact content check: @path holds exactly @s */
static int holds(char const *path, char const *s)
{
    char buf[256] ;
    int fd = open(path, O_RDONLY) ;
    if (fd < 0) return 0 ;
    ssize_t r = read(fd, buf, sizeof buf) ;
    close(fd) ;
    return r == (ssize_t)strlen(s) && !memcmp(buf, s, (size_t)r) ;
}

/* @path is a symbolic link whose target is exactly @target */
static int links_to(char const *path, char const *target)
{
    char buf[256] ;
    if (!is_type(path, S_IFLNK)) return 0 ;
    ssize_t r = readlink(path, buf, sizeof buf) ;
    return r == (ssize_t)strlen(target) && !memcmp(buf, target, (size_t)r) ;
}

/* "@dir/@name" holds exactly @s */
static int dir_holds(char const *dir, char const *name, char const *s)
{
    char p[256] ;
    int r = snprintf(p, sizeof p, "%s/%s", dir, name) ;
    TEST_ASSERT(r > 0 && (size_t)r < sizeof p, "path too long") ;
    return holds(p, s) ;
}

static void mkdir_with(char const *dir, char const *name, char const *s)
{
    char p[256] ;
    TEST_ASSERT(mkdir(dir, 0755) == 0, "mkdir failed") ;
    int r = snprintf(p, sizeof p, "%s/%s", dir, name) ;
    TEST_ASSERT(r > 0 && (size_t)r < sizeof p, "path too long") ;
    put(p, s) ;
}

static void rm_dir_with(char const *dir, char const *name)
{
    char p[256] ;
    snprintf(p, sizeof p, "%s/%s", dir, name) ;
    TEST_ASSERT(unlink(p) == 0, "cleanup unlink failed") ;
    TEST_ASSERT(rmdir(dir) == 0, "cleanup rmdir failed") ;
}

/* ---------------------------------------------------------------------------
 * success
 * ------------------------------------------------------------------------- */

/* A regular file onto an absent @dst: content at @dst, @tmp gone. */
static void test_rename_file_create(void)
{
    TEST_START("test_rename_file_create") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "f.tmp") ;
    path_in(d, sizeof d, "f") ;
    put(t, "NEW") ;
    TEST_ASSERT(file_tmp_rename(t, d) == 1, "move failed") ;
    TEST_ASSERT(is_type(d, S_IFREG), "dst not a regular file") ;
    TEST_ASSERT(holds(d, "NEW"), "dst content wrong") ;
    TEST_ASSERT(path_absent(t), "tmp still present") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

/* A regular file onto an existing @dst file: @dst replaced in one step. */
static void test_rename_file_replace(void)
{
    TEST_START("test_rename_file_replace") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "f.tmp") ;
    path_in(d, sizeof d, "f") ;
    put(d, "OLD content") ;
    put(t, "NEW") ;
    TEST_ASSERT(file_tmp_rename(t, d) == 1, "move failed") ;
    TEST_ASSERT(holds(d, "NEW"), "dst not replaced") ;
    TEST_ASSERT(path_absent(t), "tmp still present") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

/* A symbolic link: @dst is the link itself (not a copy of its target), with the
 * same target string; the target file is untouched. Also replaces an existing
 * link at @dst. */
static void test_rename_symlink(void)
{
    TEST_START("test_rename_symlink") ;
    char t[128], d[128], target[128] ;
    path_in(t, sizeof t, "l.tmp") ;
    path_in(d, sizeof d, "l") ;
    path_in(target, sizeof target, "target") ;
    put(target, "TARGET") ;
    TEST_ASSERT(symlink("old-target", d) == 0, "seed dst link failed") ;
    TEST_ASSERT(symlink(target, t) == 0, "seed tmp link failed") ;
    TEST_ASSERT(file_tmp_rename(t, d) == 1, "move failed") ;
    TEST_ASSERT(links_to(d, target), "dst is not the link with its target") ;
    TEST_ASSERT(path_absent(t), "tmp still present") ;
    TEST_ASSERT(holds(target, "TARGET"), "target modified") ;
    TEST_ASSERT(count_entries() == 2, "residue left") ;
    unlink(d) ;
    unlink(target) ;
    TEST_END() ;
}

/* A dangling symbolic link moves as well: nothing follows it. */
static void test_rename_symlink_dangling(void)
{
    TEST_START("test_rename_symlink_dangling") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "l.tmp") ;
    path_in(d, sizeof d, "l") ;
    TEST_ASSERT(symlink("nowhere/at/all", t) == 0, "seed tmp link failed") ;
    TEST_ASSERT(file_tmp_rename(t, d) == 1, "move failed") ;
    TEST_ASSERT(links_to(d, "nowhere/at/all"), "dst link target wrong") ;
    TEST_ASSERT(path_absent(t), "tmp still present") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

/* A directory with content onto an absent @dst: @dst is that directory. */
static void test_rename_dir(void)
{
    TEST_START("test_rename_dir") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "d.tmp") ;
    path_in(d, sizeof d, "d") ;
    mkdir_with(t, "inner", "INNER") ;
    TEST_ASSERT(file_tmp_rename(t, d) == 1, "move failed") ;
    TEST_ASSERT(is_type(d, S_IFDIR), "dst not a directory") ;
    TEST_ASSERT(dir_holds(d, "inner", "INNER"), "dst content wrong") ;
    TEST_ASSERT(count_entries_in(d) == 1, "dst has extra entries") ;
    TEST_ASSERT(path_absent(t), "tmp still present") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    rm_dir_with(d, "inner") ;
    TEST_END() ;
}

/* A directory onto an existing EMPTY directory: rename(2) replaces it. */
static void test_rename_dir_replace_empty(void)
{
    TEST_START("test_rename_dir_replace_empty") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "d.tmp") ;
    path_in(d, sizeof d, "d") ;
    TEST_ASSERT(mkdir(d, 0700) == 0, "seed dst failed") ;
    mkdir_with(t, "inner", "INNER") ;
    TEST_ASSERT(file_tmp_rename(t, d) == 1, "move failed") ;
    TEST_ASSERT(dir_holds(d, "inner", "INNER"), "dst not replaced") ;
    TEST_ASSERT(path_absent(t), "tmp still present") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    rm_dir_with(d, "inner") ;
    TEST_END() ;
}

/* @tmp and @dst name the same file: rename(2) does nothing and succeeds, so 1
 * is returned and the file is still there (nothing is unlinked after a
 * successful rename). */
static void test_rename_same_path(void)
{
    TEST_START("test_rename_same_path") ;
    char t[128] ;
    path_in(t, sizeof t, "same") ;
    put(t, "SAME") ;
    TEST_ASSERT(file_tmp_rename(t, t) == 1, "rename onto itself failed") ;
    TEST_ASSERT(holds(t, "SAME"), "file removed or modified") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(t) ;
    TEST_END() ;
}

/* ---------------------------------------------------------------------------
 * failure
 * ------------------------------------------------------------------------- */

/* The directory of @dst does not exist: 0, ENOENT, the regular file @tmp is
 * removed. */
static void test_fail_file_removed(void)
{
    TEST_START("test_fail_file_removed") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "f.tmp") ;
    path_in(d, sizeof d, "missing/f") ;
    put(t, "NEW") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename(t, d) == 0, "move should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(path_absent(t), "file tmp not removed") ;
    TEST_ASSERT(count_entries() == 0, "residue left") ;
    TEST_END() ;
}

/* Same failure with a symbolic link @tmp: the link is removed, its target is
 * NOT (unlink does not follow). */
static void test_fail_symlink_removed(void)
{
    TEST_START("test_fail_symlink_removed") ;
    char t[128], d[128], target[128] ;
    path_in(t, sizeof t, "l.tmp") ;
    path_in(d, sizeof d, "missing/l") ;
    path_in(target, sizeof target, "target") ;
    put(target, "TARGET") ;
    TEST_ASSERT(symlink(target, t) == 0, "seed tmp link failed") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename(t, d) == 0, "move should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(path_absent(t), "link tmp not removed") ;
    TEST_ASSERT(holds(target, "TARGET"), "link target removed or modified") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(target) ;
    TEST_END() ;
}

/* Same failure with a directory @tmp: it stays in place with its content, and
 * errno is the rename one (ENOENT), not the EISDIR the unlink() of a directory
 * gives: the errno of the failed cleanup must not leak. */
static void test_fail_dir_kept_errno_preserved(void)
{
    TEST_START("test_fail_dir_kept_errno_preserved") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "d.tmp") ;
    path_in(d, sizeof d, "missing/d") ;
    mkdir_with(t, "inner", "INNER") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename(t, d) == 0, "move should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(is_type(t, S_IFDIR), "directory tmp removed") ;
    TEST_ASSERT(dir_holds(t, "inner", "INNER"), "directory tmp content modified") ;
    TEST_ASSERT(count_entries_in(t) == 1, "directory tmp content modified") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    rm_dir_with(t, "inner") ;
    TEST_END() ;
}

/* A regular file onto an existing directory: 0, EISDIR, the file tmp removed,
 * the directory @dst untouched. */
static void test_fail_dst_is_dir(void)
{
    TEST_START("test_fail_dst_is_dir") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "f.tmp") ;
    path_in(d, sizeof d, "d") ;
    mkdir_with(d, "keep", "KEEP") ;
    put(t, "NEW") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename(t, d) == 0, "move should fail") ;
    TEST_ASSERT(errno == EISDIR, "errno should be EISDIR") ;
    TEST_ASSERT(path_absent(t), "file tmp not removed") ;
    TEST_ASSERT(dir_holds(d, "keep", "KEEP"), "dst modified") ;
    TEST_ASSERT(count_entries_in(d) == 1, "dst modified") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    rm_dir_with(d, "keep") ;
    TEST_END() ;
}

/* A directory onto an existing regular file: 0, ENOTDIR, the directory tmp
 * kept (errno not clobbered by the EISDIR of unlink), @dst untouched. */
static void test_fail_dir_onto_file(void)
{
    TEST_START("test_fail_dir_onto_file") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "d.tmp") ;
    path_in(d, sizeof d, "f") ;
    put(d, "OLD") ;
    mkdir_with(t, "inner", "INNER") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename(t, d) == 0, "move should fail") ;
    TEST_ASSERT(errno == ENOTDIR, "errno should be ENOTDIR") ;
    TEST_ASSERT(dir_holds(t, "inner", "INNER"), "directory tmp removed") ;
    TEST_ASSERT(holds(d, "OLD"), "dst modified") ;
    TEST_ASSERT(count_entries() == 2, "residue left") ;
    rm_dir_with(t, "inner") ;
    unlink(d) ;
    TEST_END() ;
}

/* @tmp does not exist: 0, ENOENT, @dst untouched. */
static void test_fail_tmp_absent(void)
{
    TEST_START("test_fail_tmp_absent") ;
    char t[128], d[128] ;
    path_in(t, sizeof t, "absent.tmp") ;
    path_in(d, sizeof d, "f") ;
    put(d, "OLD") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename(t, d) == 0, "move should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(holds(d, "OLD"), "dst modified") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

int main(void)
{
    umask(022) ;
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_tmp_rename test suite ===\n\n") ;

    test_rename_file_create() ;
    test_rename_file_replace() ;
    test_rename_symlink() ;
    test_rename_symlink_dangling() ;
    test_rename_dir() ;
    test_rename_dir_replace_empty() ;
    test_rename_same_path() ;
    test_fail_file_removed() ;
    test_fail_symlink_removed() ;
    test_fail_dir_kept_errno_preserved() ;
    test_fail_dst_is_dir() ;
    test_fail_dir_onto_file() ;
    test_fail_tmp_absent() ;

    TEST_ASSERT(rmdir(tdir) == 0, "test dir not empty at the end") ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
