/*
 * file_tmp.c
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

/* Tests for the two halves of an atomic write: file_tmp_open (mkstemp then
 * cloexec_fd, optional fchown, exact fchmod independent of umask, full cleanup on
 * failure) and file_tmp_rename_at (checked close then rename, the temporary removed
 * on any failure, destination untouched). Every check is made on the effect:
 * stat of the files, directory listing, fd state, exact errno.
 *
 * fchmod() and cloexec_fd() never fail on a descriptor the caller just created,
 * so their failure branches are reached by interposing them in this executable:
 * when fchmod_fail/cloexec_fail is set the call fails with that errno, otherwise
 * fchmod forwards to the real syscall and cloexec_fd performs the same
 * F_GETFD/F_SETFD sequence as the library (whose own copy is tested in
 * src/fd/test/cloexec_fd.c). The fchown
 * failure branch is reached for real, by asking for an id the kernel refuses:
 * EINVAL when the id is not mapped in the current user namespace, EPERM for an
 * unprivileged caller in the initial namespace. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <oblibs/files.h>
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
#define TEST_SKIP(why) printf(" SKIP (%s)\n", why)

/* an id that is neither ours nor, in a rootless namespace, mapped */
#define FOREIGN_ID 4242424u

static char tdir[] = "/tmp/oblibs_ftmp_XXXXXX" ;

/* fchmod interposition, see the header comment */
static int fchmod_fail = 0 ;

int fchmod(int fd, mode_t mode)
{
    if (fchmod_fail) {
        errno = fchmod_fail ;
        return -1 ;
    }
    return (int)syscall(SYS_fchmod, fd, mode) ;
}

/* cloexec_fd interposition, see the header comment */
static int cloexec_fail = 0 ;

int cloexec_fd(int fd)
{
    if (cloexec_fail) {
        errno = cloexec_fail ;
        return 0 ;
    }
    int f = fcntl(fd, F_GETFD) ;
    if (f < 0 || fcntl(fd, F_SETFD, f | FD_CLOEXEC) < 0)
        return 0 ;
    return 1 ;
}

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

/* the descriptor number the next open() will return (lowest free one) */
static int next_fd(void)
{
    int fd = open("/dev/null", O_RDONLY) ;
    TEST_ASSERT(fd >= 0, "open /dev/null failed") ;
    close(fd) ;
    return fd ;
}

static int fd_is_closed(int fd)
{
    errno = 0 ;
    int r = fcntl(fd, F_GETFD) ;
    int closed = r == -1 && errno == EBADF ;
    errno = 0 ;
    return closed ;
}

static int path_absent(char const *path)
{
    struct stat st ;
    int absent = lstat(path, &st) == -1 && errno == ENOENT ;
    errno = 0 ;
    return absent ;
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

/* Is @id mapped in /proc/self/<map> (uid_map or gid_map)? A missing file means
 * no user namespace support, every id is then valid. */
static int id_mapped(char const *map, unsigned long id)
{
    char p[32] ;
    snprintf(p, sizeof p, "/proc/self/%s", map) ;
    FILE *f = fopen(p, "r") ;
    if (!f) { errno = 0 ; return 1 ; }
    unsigned long in, out, cnt ;
    int found = 0 ;
    while (fscanf(f, "%lu %lu %lu", &in, &out, &cnt) == 3)
        if (id >= in && id - in < cnt) { found = 1 ; break ; }
    fclose(f) ;
    return found ;
}

/* errno the kernel gives to fchown(FOREIGN_ID) for this map, 0 if it would
 * succeed (real root in the initial namespace) */
static int foreign_errno(char const *map)
{
    if (!id_mapped(map, FOREIGN_ID)) return EINVAL ;
    if (geteuid()) return EPERM ;
    return 0 ;
}

/* ---------------------------------------------------------------------------
 * file_tmp_open
 * ------------------------------------------------------------------------- */

/* Success: a descriptor is returned, the template is rewritten in place and names the file
 * the descriptor refers to, the descriptor is read-write and close-on-exec. */
static void test_open_basic(void)
{
    TEST_START("test_open_basic") ;
    char t[128] ;
    path_in(t, sizeof t, "b.XXXXXX") ;
    size_t tl = strlen(t) ;

    int fd = file_tmp_open(t, 0644, (uid_t)-1, (gid_t)-1) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open failed") ;
    TEST_ASSERT(strlen(t) == tl, "template length changed") ;
    TEST_ASSERT(strcmp(t + tl - 6, "XXXXXX") != 0, "template not rewritten") ;

    struct stat sp, sf ;
    TEST_ASSERT(stat(t, &sp) == 0, "rewritten template does not exist") ;
    TEST_ASSERT(fstat(fd, &sf) == 0, "fstat failed") ;
    TEST_ASSERT(sp.st_ino == sf.st_ino && sp.st_dev == sf.st_dev, "fd is not the template file") ;
    TEST_ASSERT(S_ISREG(sf.st_mode), "not a regular file") ;
    TEST_ASSERT(sf.st_size == 0, "temporary not empty") ;

    int fdfl = fcntl(fd, F_GETFD) ;
    TEST_ASSERT(fdfl >= 0 && (fdfl & FD_CLOEXEC), "FD_CLOEXEC not set") ;
    TEST_ASSERT((fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDWR, "fd not O_RDWR") ;

    TEST_ASSERT(write(fd, "data", 4) == 4, "write through fd failed") ;
    char rb[8] ;
    TEST_ASSERT(pread(fd, rb, sizeof rb, 0) == 4 && !memcmp(rb, "data", 4), "read back failed") ;

    TEST_ASSERT(count_entries() == 1, "exactly one file expected") ;
    close(fd) ;
    unlink(t) ;
    TEST_END() ;
}

/* The mode is applied verbatim: umask is not involved (fchmod, not open). Each
 * mode is checked under umask 077 (would strip group/other) and umask 0 (would
 * leave mkstemp's 0600 if fchmod were missing). */
static void test_open_exact_mode(void)
{
    TEST_START("test_open_exact_mode") ;
    mode_t const modes[] = { 0644, 0755, 0640, 0000, 0777 } ;
    mode_t const masks[] = { 077, 0 } ;
    mode_t saved = umask(0) ;

    for (size_t m = 0 ; m < sizeof masks / sizeof *masks ; m++) {
        umask(masks[m]) ;
        for (size_t i = 0 ; i < sizeof modes / sizeof *modes ; i++) {
            char t[128] ;
            path_in(t, sizeof t, "m.XXXXXX") ;
            int fd = file_tmp_open(t, modes[i], (uid_t)-1, (gid_t)-1) ;
            TEST_ASSERT(fd >= 0, "file_tmp_open failed") ;
            struct stat st ;
            TEST_ASSERT(stat(t, &st) == 0, "stat failed") ;
            TEST_ASSERT((st.st_mode & 07777) == modes[i], "mode not applied verbatim") ;
            close(fd) ;
            unlink(t) ;
        }
    }
    umask(saved) ;
    TEST_ASSERT(count_entries() == 0, "residue left") ;
    TEST_END() ;
}

/* uid/gid -1: no chown, the file belongs to the caller. */
static void test_open_owner_unchanged(void)
{
    TEST_START("test_open_owner_unchanged") ;
    char t[128] ;
    path_in(t, sizeof t, "o.XXXXXX") ;
    int fd = file_tmp_open(t, 0600, (uid_t)-1, (gid_t)-1) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open failed") ;
    struct stat st ;
    TEST_ASSERT(fstat(fd, &st) == 0, "fstat failed") ;
    TEST_ASSERT(st.st_uid == geteuid(), "uid changed") ;
    TEST_ASSERT(st.st_gid == getegid(), "gid changed") ;
    close(fd) ;
    unlink(t) ;
    TEST_END() ;
}

/* Explicit own uid/gid, with a set-uid mode: chown runs BEFORE chmod, so the
 * S_ISUID bit fchmod sets survives (chown clears it on a regular file). */
static void test_open_owner_self_keeps_suid(void)
{
    TEST_START("test_open_owner_self_keeps_suid") ;
    char t[128] ;
    path_in(t, sizeof t, "s.XXXXXX") ;
    int fd = file_tmp_open(t, 04755, geteuid(), getegid()) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open failed") ;
    struct stat st ;
    TEST_ASSERT(fstat(fd, &st) == 0, "fstat failed") ;
    TEST_ASSERT(st.st_uid == geteuid() && st.st_gid == getegid(), "owner wrong") ;
    TEST_ASSERT((st.st_mode & 07777) == 04755, "S_ISUID lost: chown ran after chmod") ;
    close(fd) ;
    unlink(t) ;
    TEST_END() ;
}

/* Effective group change to one of the caller's supplementary groups. Needs a
 * mapped supplementary group: absent in a rootless single-id namespace. */
static void test_open_group_change(void)
{
    TEST_START("test_open_group_change") ;
    gid_t groups[256] ;
    int n = getgroups(256, groups) ;
    TEST_ASSERT(n >= 0, "getgroups failed") ;
    gid_t target = (gid_t)-1 ;
    for (int i = 0 ; i < n ; i++)
        if (groups[i] != getegid() && groups[i] != 65534 && id_mapped("gid_map", groups[i])) {
            target = groups[i] ;
            break ;
        }
    if (target == (gid_t)-1) {
        TEST_SKIP("no mapped supplementary group") ;
        return ;
    }
    char t[128] ;
    path_in(t, sizeof t, "g.XXXXXX") ;
    int fd = file_tmp_open(t, 0640, (uid_t)-1, target) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open failed") ;
    struct stat st ;
    TEST_ASSERT(fstat(fd, &st) == 0, "fstat failed") ;
    TEST_ASSERT(st.st_gid == target, "gid not changed") ;
    TEST_ASSERT(st.st_uid == geteuid(), "uid changed") ;
    TEST_ASSERT((st.st_mode & 07777) == 0640, "mode wrong") ;
    close(fd) ;
    unlink(t) ;
    TEST_END() ;
}

/* Invalid templates: mkstemp fails with EINVAL, nothing is created, no fd. */
static void test_open_invalid_template(void)
{
    TEST_START("test_open_invalid_template") ;
    char const *bad[] = { "XXXXX", "x.XXXXXXy", "x", "" } ;
    for (size_t i = 0 ; i < sizeof bad / sizeof *bad ; i++) {
        char t[128] ;
        path_in(t, sizeof t, bad[i]) ;
        int nfd = next_fd() ;
        errno = 0 ;
        TEST_ASSERT(file_tmp_open(t, 0644, (uid_t)-1, (gid_t)-1) == -1, "should fail") ;
        TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
        TEST_ASSERT(fd_is_closed(nfd), "a descriptor was left open") ;
        TEST_ASSERT(count_entries() == 0, "a file was created") ;
    }
    TEST_END() ;
}

/* Template in a missing directory: ENOENT. */
static void test_open_missing_dir(void)
{
    TEST_START("test_open_missing_dir") ;
    char t[128] ;
    path_in(t, sizeof t, "nodir/f.XXXXXX") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_open(t, 0644, (uid_t)-1, (gid_t)-1) == -1, "should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(count_entries() == 0, "a file was created") ;
    TEST_END() ;
}

/* fchown refused (uid alone, then gid alone): -1, the kernel errno preserved
 * through the cleanup, the descriptor closed, the temporary removed. Doing each
 * id alone pins the "uid OR gid" test that decides whether to chown at all. */
static void test_open_chown_fail(void)
{
    TEST_START("test_open_chown_fail") ;
    int want_u = foreign_errno("uid_map") ;
    int want_g = foreign_errno("gid_map") ;
    if (!want_u || !want_g) {
        TEST_SKIP("privileged caller, chown cannot be refused") ;
        return ;
    }
    for (int k = 0 ; k < 2 ; k++) {
        char t[128] ;
        path_in(t, sizeof t, "c.XXXXXX") ;
        uid_t u = k ? (uid_t)-1 : (uid_t)FOREIGN_ID ;
        gid_t g = k ? (gid_t)FOREIGN_ID : (gid_t)-1 ;
        int nfd = next_fd() ;
        errno = 0 ;
        TEST_ASSERT(file_tmp_open(t, 0644, u, g) == -1, "should fail") ;
        TEST_ASSERT(errno == (k ? want_g : want_u), "errno not the fchown errno") ;
        TEST_ASSERT(fd_is_closed(nfd), "descriptor leaked") ;
        TEST_ASSERT(count_entries() == 0, "temporary not removed") ;
    }
    TEST_END() ;
}

/* fchmod failure (interposed): -1, errno preserved, fd closed, temp removed. */
static void test_open_chmod_fail(void)
{
    TEST_START("test_open_chmod_fail") ;
    char t[128] ;
    path_in(t, sizeof t, "h.XXXXXX") ;
    int nfd = next_fd() ;
    fchmod_fail = EIO ;
    errno = 0 ;
    int r = file_tmp_open(t, 0644, (uid_t)-1, (gid_t)-1) ;
    int e = errno ;
    fchmod_fail = 0 ;
    errno = e ;
    TEST_ASSERT(r == -1, "should fail") ;
    TEST_ASSERT(errno == EIO, "errno not the fchmod errno") ;
    TEST_ASSERT(fd_is_closed(nfd), "descriptor leaked") ;
    TEST_ASSERT(count_entries() == 0, "temporary not removed") ;
    TEST_END() ;
}

/* cloexec_fd failure (interposed): -1, errno preserved, fd closed, temp
 * removed. */
static void test_open_cloexec_fail(void)
{
    TEST_START("test_open_cloexec_fail") ;
    char t[128] ;
    path_in(t, sizeof t, "e.XXXXXX") ;
    int nfd = next_fd() ;
    cloexec_fail = EBADF ;
    errno = 0 ;
    int r = file_tmp_open(t, 0644, (uid_t)-1, (gid_t)-1) ;
    int e = errno ;
    cloexec_fail = 0 ;
    errno = e ;
    TEST_ASSERT(r == -1, "should fail") ;
    TEST_ASSERT(errno == EBADF, "errno not the cloexec_fd errno") ;
    TEST_ASSERT(fd_is_closed(nfd), "descriptor leaked") ;
    TEST_ASSERT(count_entries() == 0, "temporary not removed") ;
    TEST_END() ;
}

/* ---------------------------------------------------------------------------
 * file_tmp_rename_at
 * ------------------------------------------------------------------------- */

/* a temporary holding @s, its open fd returned, its path in @t */
static int mk_tmp(char *t, size_t max, char const *dir, char const *s)
{
    int r = snprintf(t, max, "%s/t.XXXXXX", dir) ;
    TEST_ASSERT(r > 0 && (size_t)r < max, "path too long") ;
    int fd = mkstemp(t) ;
    TEST_ASSERT(fd >= 0, "mkstemp failed") ;
    size_t len = strlen(s) ;
    TEST_ASSERT(write(fd, s, len) == (ssize_t)len, "tmp write failed") ;
    return fd ;
}

/* Success over an existing destination: 1, dst is the temporary (same inode,
 * new content), the temporary name is gone, the fd is closed. */
static void test_rename_at_replace(void)
{
    TEST_START("test_rename_at_replace") ;
    char t[128], d[128] ;
    path_in(d, sizeof d, "dst") ;
    put(d, "OLD") ;
    int fd = mk_tmp(t, sizeof t, tdir, "NEW") ;
    struct stat st_t ;
    TEST_ASSERT(fstat(fd, &st_t) == 0, "fstat failed") ;

    TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 1, "rename failed") ;
    TEST_ASSERT(fd_is_closed(fd), "fd not closed") ;
    TEST_ASSERT(path_absent(t), "temporary still present") ;
    struct stat st_d ;
    TEST_ASSERT(stat(d, &st_d) == 0, "dst missing") ;
    TEST_ASSERT(st_d.st_ino == st_t.st_ino, "dst is not the temporary") ;
    TEST_ASSERT(holds(d, "NEW"), "dst content wrong") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

/* Success with no previous destination. */
static void test_rename_at_create(void)
{
    TEST_START("test_rename_at_create") ;
    char t[128], d[128] ;
    path_in(d, sizeof d, "fresh") ;
    int fd = mk_tmp(t, sizeof t, tdir, "X") ;
    TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 1, "rename failed") ;
    TEST_ASSERT(fd_is_closed(fd), "fd not closed") ;
    TEST_ASSERT(path_absent(t), "temporary still present") ;
    TEST_ASSERT(holds(d, "X"), "dst content wrong") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

/* close() fails (descriptor already closed, then -1): 0, EBADF, no rename, the
 * temporary removed, dst untouched. */
static void test_rename_at_close_fail(void)
{
    TEST_START("test_rename_at_close_fail") ;
    for (int k = 0 ; k < 2 ; k++) {
        char t[128], d[128] ;
        path_in(d, sizeof d, "dst") ;
        put(d, "OLD") ;
        int fd = mk_tmp(t, sizeof t, tdir, "NEW") ;
        TEST_ASSERT(close(fd) == 0, "close failed") ;
        if (k) fd = -1 ;
        errno = 0 ;
        TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 0, "rename should fail") ;
        TEST_ASSERT(errno == EBADF, "errno should be EBADF") ;
        TEST_ASSERT(path_absent(t), "temporary not removed") ;
        TEST_ASSERT(holds(d, "OLD"), "dst modified") ;
        TEST_ASSERT(count_entries() == 1, "residue left") ;
        unlink(d) ;
    }
    TEST_END() ;
}

/* rename() fails: destination in a missing directory (ENOENT), destination a
 * non-empty directory (EISDIR). 0, exact errno, fd closed, temporary removed,
 * the directory destination untouched. */
static void test_rename_at_fail(void)
{
    TEST_START("test_rename_at_fail") ;
    char t[128], d[128] ;

    path_in(d, sizeof d, "nodir/dst") ;
    int fd = mk_tmp(t, sizeof t, tdir, "NEW") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 0, "rename should fail") ;
    TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
    TEST_ASSERT(fd_is_closed(fd), "fd not closed") ;
    TEST_ASSERT(path_absent(t), "temporary not removed") ;
    TEST_ASSERT(count_entries() == 0, "residue left") ;

    char sub[128], inner[160] ;
    path_in(sub, sizeof sub, "dir") ;
    TEST_ASSERT(mkdir(sub, 0755) == 0, "mkdir failed") ;
    snprintf(inner, sizeof inner, "%s/keep", sub) ;
    put(inner, "KEEP") ;
    fd = mk_tmp(t, sizeof t, tdir, "NEW") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename_at(fd, t, sub) == 0, "rename should fail") ;
    TEST_ASSERT(errno == EISDIR, "errno should be EISDIR") ;
    TEST_ASSERT(fd_is_closed(fd), "fd not closed") ;
    TEST_ASSERT(path_absent(t), "temporary not removed") ;
    TEST_ASSERT(holds(inner, "KEEP"), "directory destination modified") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(inner) ;
    rmdir(sub) ;
    TEST_END() ;
}

/* rename() across filesystems: EXDEV. Needs a writable /dev/shm on another
 * filesystem than /tmp. */
static void test_rename_at_exdev(void)
{
    TEST_START("test_rename_at_exdev") ;
    struct stat a, b ;
    if (stat("/dev/shm", &b) < 0 || stat(tdir, &a) < 0 || a.st_dev == b.st_dev || access("/dev/shm", W_OK) < 0) {
        errno = 0 ;
        TEST_SKIP("no writable /dev/shm on a separate filesystem") ;
        return ;
    }
    char shm[] = "/dev/shm/oblibs_ftmp_XXXXXX" ;
    TEST_ASSERT(mkdtemp(shm) != NULL, "mkdtemp /dev/shm failed") ;
    char t[128], d[128] ;
    path_in(d, sizeof d, "dst") ;
    put(d, "OLD") ;
    int fd = mk_tmp(t, sizeof t, shm, "NEW") ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 0, "rename should fail") ;
    TEST_ASSERT(errno == EXDEV, "errno should be EXDEV") ;
    TEST_ASSERT(fd_is_closed(fd), "fd not closed") ;
    TEST_ASSERT(path_absent(t), "temporary not removed") ;
    TEST_ASSERT(count_entries_in(shm) == 0, "residue left in /dev/shm") ;
    TEST_ASSERT(holds(d, "OLD"), "dst modified") ;
    unlink(d) ;
    rmdir(shm) ;
    TEST_END() ;
}

/* The two halves together, the way file_write_atomic_at uses them. */
static void test_open_rename_at_roundtrip(void)
{
    TEST_START("test_open_rename_at_roundtrip") ;
    char t[128], d[128] ;
    path_in(d, sizeof d, "final") ;
    put(d, "OLD") ;
    path_in(t, sizeof t, "final.XXXXXX") ;
    int fd = file_tmp_open(t, 0640, (uid_t)-1, (gid_t)-1) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open failed") ;
    TEST_ASSERT(write(fd, "NEW", 3) == 3, "write failed") ;
    TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 1, "rename failed") ;
    struct stat st ;
    TEST_ASSERT(stat(d, &st) == 0 && (st.st_mode & 07777) == 0640, "final mode wrong") ;
    TEST_ASSERT(holds(d, "NEW"), "final content wrong") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

/* ---------------------------------------------------------------------------
 * file_tmp_open_at
 * ------------------------------------------------------------------------- */

#define FILL 0x5a

/* @dir in @out, with a trailing '/' if @slash */
static void dir_form(char *out, size_t max, char const *dir, int slash)
{
    int r = snprintf(out, max, "%s%s", dir, slash ? "/" : "") ;
    TEST_ASSERT(r > 0 && (size_t)r < max, "path too long") ;
}

/* all @len bytes of @b equal FILL */
static int untouched(char const *b, size_t len)
{
    for (size_t i = 0 ; i < len ; i++)
        if ((unsigned char)b[i] != FILL) return 0 ;
    return 1 ;
}

/* @tmp is exactly "<tdir>/" followed by 6 non-'/' characters other than
 * "XXXXXX", and names the file @fd refers to, inside tdir */
static void check_composed(char const *tmp, int fd)
{
    size_t dl = strlen(tdir) ;
    TEST_ASSERT(strlen(tmp) == dl + 1 + 6, "composed length is not <dir>/XXXXXX") ;
    TEST_ASSERT(!strncmp(tmp, tdir, dl), "prefix is not the directory") ;
    TEST_ASSERT(tmp[dl] == '/', "separator missing") ;
    for (size_t i = dl + 1 ; i < dl + 7 ; i++)
        TEST_ASSERT(tmp[i] != '/', "separator doubled or misplaced") ;
    TEST_ASSERT(strcmp(tmp + dl + 1, "XXXXXX") != 0, "template not rewritten") ;
    struct stat sp, sf ;
    TEST_ASSERT(stat(tmp, &sp) == 0, "composed path does not exist") ;
    TEST_ASSERT(fstat(fd, &sf) == 0, "fstat failed") ;
    TEST_ASSERT(sp.st_ino == sf.st_ino && sp.st_dev == sf.st_dev, "fd is not the composed file") ;
    TEST_ASSERT(S_ISREG(sf.st_mode), "not a regular file") ;
    TEST_ASSERT(count_entries() == 1, "file not created in dir") ;
}

/* Template composed with and without a trailing '/' on @dir: "<dir>/" exactly
 * once, 6 random characters, the file exists in @dir, the fd is the file. */
static void test_open_at_compose(void)
{
    TEST_START("test_open_at_compose") ;
    for (int slash = 0 ; slash < 2 ; slash++) {
        char d[128], t[128] ;
        dir_form(d, sizeof d, tdir, slash) ;
        int fd = file_tmp_open_at(t, sizeof t, d, 0644, (uid_t)-1, (gid_t)-1) ;
        TEST_ASSERT(fd >= 0, "file_tmp_open_at failed") ;
        check_composed(t, fd) ;
        close(fd) ;
        TEST_ASSERT(unlink(t) == 0, "unlink failed") ;
    }
    TEST_ASSERT(count_entries() == 0, "residue left") ;
    TEST_END() ;
}

/* ENAMETOOLONG: tmplen 0, tmplen == needed - 1 fail, nothing created, every
 * byte of @tmp unchanged; tmplen == needed succeeds and writes no byte past
 * tmplen. needed = strlen(dir) + sep + 6 + 1, checked with and without a
 * trailing '/' (sep 1 then 0). The success buffer is heap-allocated at exactly
 * tmplen bytes so ASan catches any overrun; a canary byte guards the stack
 * copy as well. */
static void test_open_at_toolong(void)
{
    TEST_START("test_open_at_toolong") ;
    for (int slash = 0 ; slash < 2 ; slash++) {
        char d[128], t[160] ;
        dir_form(d, sizeof d, tdir, slash) ;
        size_t need = strlen(tdir) + 1 + 6 + 1 ;
        size_t const fails[] = { 0, 1, need - 1 } ;

        for (size_t k = 0 ; k < sizeof fails / sizeof *fails ; k++) {
            memset(t, FILL, sizeof t) ;
            errno = 0 ;
            TEST_ASSERT(file_tmp_open_at(t, fails[k], d, 0644, (uid_t)-1, (gid_t)-1) == -1, "should fail") ;
            TEST_ASSERT(errno == ENAMETOOLONG, "errno should be ENAMETOOLONG") ;
            TEST_ASSERT(untouched(t, sizeof t), "tmp modified") ;
            TEST_ASSERT(count_entries() == 0, "a file was created") ;
        }

        /* exact fit, stack buffer with canary */
        memset(t, FILL, sizeof t) ;
        int fd = file_tmp_open_at(t, need, d, 0644, (uid_t)-1, (gid_t)-1) ;
        TEST_ASSERT(fd >= 0, "exact fit should succeed") ;
        TEST_ASSERT(untouched(t + need, sizeof t - need), "wrote past tmplen") ;
        check_composed(t, fd) ;
        close(fd) ;
        TEST_ASSERT(unlink(t) == 0, "unlink failed") ;

        /* exact fit, heap buffer of exactly need bytes */
        char *h = malloc(need) ;
        TEST_ASSERT(h != NULL, "malloc failed") ;
        fd = file_tmp_open_at(h, need, d, 0644, (uid_t)-1, (gid_t)-1) ;
        TEST_ASSERT(fd >= 0, "exact fit (heap) should succeed") ;
        check_composed(h, fd) ;
        close(fd) ;
        TEST_ASSERT(unlink(h) == 0, "unlink failed") ;
        free(h) ;
    }
    TEST_ASSERT(count_entries() == 0, "residue left") ;
    TEST_END() ;
}

/* Empty @dir: -1, EINVAL, @tmp unchanged, no descriptor, nothing created. */
static void test_open_at_empty_dir(void)
{
    TEST_START("test_open_at_empty_dir") ;
    char t[64] ;
    memset(t, FILL, sizeof t) ;
    int nfd = next_fd() ;
    errno = 0 ;
    TEST_ASSERT(file_tmp_open_at(t, sizeof t, "", 0644, (uid_t)-1, (gid_t)-1) == -1, "should fail") ;
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL") ;
    TEST_ASSERT(untouched(t, sizeof t), "tmp modified") ;
    TEST_ASSERT(fd_is_closed(nfd), "a descriptor was left open") ;
    TEST_ASSERT(count_entries() == 0, "a file was created") ;
    TEST_END() ;
}

/* Mode applied exactly: 0640 (plus 0777 and 0000, the two extremes) under
 * umask 077 and under umask 0. */
static void test_open_at_exact_mode(void)
{
    TEST_START("test_open_at_exact_mode") ;
    mode_t const modes[] = { 0640, 0777, 0000 } ;
    mode_t const masks[] = { 077, 0 } ;
    mode_t saved = umask(0) ;
    for (size_t m = 0 ; m < sizeof masks / sizeof *masks ; m++) {
        umask(masks[m]) ;
        for (size_t i = 0 ; i < sizeof modes / sizeof *modes ; i++) {
            char t[128] ;
            int fd = file_tmp_open_at(t, sizeof t, tdir, modes[i], (uid_t)-1, (gid_t)-1) ;
            TEST_ASSERT(fd >= 0, "file_tmp_open_at failed") ;
            struct stat st ;
            TEST_ASSERT(stat(t, &st) == 0, "stat failed") ;
            TEST_ASSERT((st.st_mode & 0777) == modes[i], "mode not applied exactly") ;
            TEST_ASSERT((st.st_mode & 07000) == 0, "special bits set") ;
            close(fd) ;
            TEST_ASSERT(unlink(t) == 0, "unlink failed") ;
        }
    }
    umask(saved) ;
    TEST_ASSERT(count_entries() == 0, "residue left") ;
    TEST_END() ;
}

/* The descriptor is close-on-exec and read-write. */
static void test_open_at_cloexec(void)
{
    TEST_START("test_open_at_cloexec") ;
    char t[128] ;
    int fd = file_tmp_open_at(t, sizeof t, tdir, 0600, (uid_t)-1, (gid_t)-1) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open_at failed") ;
    int fdfl = fcntl(fd, F_GETFD) ;
    TEST_ASSERT(fdfl >= 0 && (fdfl & FD_CLOEXEC), "FD_CLOEXEC not set") ;
    TEST_ASSERT((fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDWR, "fd not O_RDWR") ;
    close(fd) ;
    TEST_ASSERT(unlink(t) == 0, "unlink failed") ;
    TEST_END() ;
}

/* Missing @dir (with and without trailing '/'): -1, ENOENT, nothing created,
 * no descriptor left. */
static void test_open_at_missing_dir(void)
{
    TEST_START("test_open_at_missing_dir") ;
    char nd[128] ;
    path_in(nd, sizeof nd, "nodir") ;
    for (int slash = 0 ; slash < 2 ; slash++) {
        char d[128], t[128] ;
        dir_form(d, sizeof d, nd, slash) ;
        int nfd = next_fd() ;
        errno = 0 ;
        TEST_ASSERT(file_tmp_open_at(t, sizeof t, d, 0644, (uid_t)-1, (gid_t)-1) == -1, "should fail") ;
        TEST_ASSERT(errno == ENOENT, "errno should be ENOENT") ;
        TEST_ASSERT(fd_is_closed(nfd), "a descriptor was left open") ;
        TEST_ASSERT(count_entries() == 0, "a file was created") ;
    }
    TEST_END() ;
}

/* uid and gid are forwarded to file_tmp_open: a foreign id (uid alone, then
 * gid alone) makes fchown fail with the kernel errno, the temporary removed. */
static void test_open_at_owner_forwarded(void)
{
    TEST_START("test_open_at_owner_forwarded") ;
    int want_u = foreign_errno("uid_map") ;
    int want_g = foreign_errno("gid_map") ;
    if (!want_u || !want_g) {
        TEST_SKIP("privileged caller, chown cannot be refused") ;
        return ;
    }
    for (int k = 0 ; k < 2 ; k++) {
        char t[128] ;
        uid_t u = k ? (uid_t)-1 : (uid_t)FOREIGN_ID ;
        gid_t g = k ? (gid_t)FOREIGN_ID : (gid_t)-1 ;
        int nfd = next_fd() ;
        errno = 0 ;
        TEST_ASSERT(file_tmp_open_at(t, sizeof t, tdir, 0644, u, g) == -1, "should fail") ;
        TEST_ASSERT(errno == (k ? want_g : want_u), "errno not the fchown errno") ;
        TEST_ASSERT(fd_is_closed(nfd), "descriptor leaked") ;
        TEST_ASSERT(count_entries() == 0, "temporary not removed") ;
    }
    TEST_END() ;
}

/* Round trip with file_tmp_rename_at: content written through the fd lands in the
 * final file with the requested mode, no temporary is left in @dir. */
static void test_open_at_rename_at_roundtrip(void)
{
    TEST_START("test_open_at_rename_at_roundtrip") ;
    char t[128], d[128], dd[128] ;
    path_in(d, sizeof d, "final") ;
    put(d, "OLD") ;
    dir_form(dd, sizeof dd, tdir, 1) ;
    int fd = file_tmp_open_at(t, sizeof t, dd, 0640, (uid_t)-1, (gid_t)-1) ;
    TEST_ASSERT(fd >= 0, "file_tmp_open_at failed") ;
    TEST_ASSERT(count_entries() == 2, "temporary not created beside final") ;
    TEST_ASSERT(write(fd, "NEW content", 11) == 11, "write failed") ;
    TEST_ASSERT(file_tmp_rename_at(fd, t, d) == 1, "rename failed") ;
    TEST_ASSERT(fd_is_closed(fd), "fd not closed") ;
    TEST_ASSERT(path_absent(t), "temporary still present") ;
    struct stat st ;
    TEST_ASSERT(stat(d, &st) == 0 && (st.st_mode & 07777) == 0640, "final mode wrong") ;
    TEST_ASSERT(holds(d, "NEW content"), "final content wrong") ;
    TEST_ASSERT(count_entries() == 1, "residue left") ;
    unlink(d) ;
    TEST_END() ;
}

int main(void)
{
    umask(022) ;
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_tmp_open / file_tmp_rename_at test suite ===\n\n") ;

    test_open_basic() ;
    test_open_exact_mode() ;
    test_open_owner_unchanged() ;
    test_open_owner_self_keeps_suid() ;
    test_open_group_change() ;
    test_open_invalid_template() ;
    test_open_missing_dir() ;
    test_open_chown_fail() ;
    test_open_chmod_fail() ;
    test_open_cloexec_fail() ;
    test_rename_at_replace() ;
    test_rename_at_create() ;
    test_rename_at_close_fail() ;
    test_rename_at_fail() ;
    test_rename_at_exdev() ;
    test_open_rename_at_roundtrip() ;
    test_open_at_compose() ;
    test_open_at_toolong() ;
    test_open_at_empty_dir() ;
    test_open_at_exact_mode() ;
    test_open_at_cloexec() ;
    test_open_at_missing_dir() ;
    test_open_at_owner_forwarded() ;
    test_open_at_rename_at_roundtrip() ;

    TEST_ASSERT(rmdir(tdir) == 0, "test dir not empty at the end") ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
