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
 * except according to the terms contained in the LICENSE file./
 *
 * Native replacement for skalibs hiercopy/hiercopy_internal (djbunix): a
 * recursive hierarchy copy (cp -a semantics) preserving type, mode and
 * ownership. It follows the skalibs state machine but uses opendir/readdir +
 * strbuf (the slurp-then-recurse pattern of dir_destroy) instead of sals/satmp,
 * so no skalibs/stralloc dependency remains. Symlinks are copied verbatim,
 * never followed (lstat). It diverges from skalibs in two ways: a child copy
 * failure is propagated (skalibs swallows it), and the copy is idempotent into
 * a populated destination (existing regular files truncated, existing dirs
 * tolerated, existing symlinks replaced). Credit: skalibs (ISC, Laurent
 * Bercot).
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

/* Ensure p->s is NUL-terminated at index p->len (the NUL is not counted in len).
 * strbuf does not zero its slack, so we write it explicitly. */
static int term(strbuf *p)
{
    if (!strbuf_reserve(p, p->len + 1))
        return 0 ;
    p->s[p->len] = 0 ;
    return 1 ;
}

static int copy_node(strbuf *src, strbuf *dst) ;

/* Copy the directory src->s (mode @mode) to dst->s, reusing src/dst as scratch
 * for child paths. Mirrors skalibs dircopy: mkdir with S_IRWXU first (so we can
 * write children even if @mode is read-only), copy every child, then chmod to
 * the real @mode last. */
static int dircopy(strbuf *src, strbuf *dst, mode_t mode)
{
    if (mkdir(dst->s, S_IRWXU) < 0) {
        struct stat st ;
        if (errno != EEXIST) return 0 ;
        if (stat(dst->s, &st) < 0) return 0 ;
        if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR ; return 0 ; }
    }

    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;   /* entries, NUL-separated */
    DIR *d = opendir(src->s) ;
    if (!d) return 0 ;
    for (;;) {
        errno = 0 ;
        struct dirent *de = readdir(d) ;
        if (!de) break ;
        char const *nm = de->d_name ;
        if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0)))
            continue ;
        if (!strbuf_catb(&names, nm, strlen(nm) + 1)) { dir_close(d) ; return 0 ; }
    }
    if (errno) { dir_close(d) ; return 0 ; }   /* readdir failure */
    dir_close(d) ;

    size_t sbase = src->len, dbase = dst->len ;
    for (size_t off = 0 ; off < names.len ; ) {
        char const *nm = names.s + off ;
        size_t nl = strlen(nm) ;
        src->len = sbase ; dst->len = dbase ;
        if (!strbuf_catb(src, "/", 1) || !strbuf_catb(src, nm, nl) || !term(src))
            return 0 ;
        if (!strbuf_catb(dst, "/", 1) || !strbuf_catb(dst, nm, nl) || !term(dst))
            return 0 ;
        if (!copy_node(src, dst))
            return 0 ;
        off += nl + 1 ;
    }
    src->len = sbase ; dst->len = dbase ;
    if (!term(src) || !term(dst))
        return 0 ;

    if (chmod(dst->s, mode) < 0)
        return 0 ;
    return 1 ;
}

/* Copy the single node src->s to dst->s, dispatching on its (lstat) type.
 * src/dst are NUL-terminated paths, reused as scratch by dircopy. */
static int copy_node(strbuf *src, strbuf *dst)
{
    struct stat st ;
    if (lstat(src->s, &st) < 0)
        return 0 ;

    if (S_ISREG(st.st_mode)) {
        if (!file_copy(src->s, dst->s, st.st_mode))
            return 0 ;
    }
    else if (S_ISDIR(st.st_mode)) {
        if (!dircopy(src, dst, st.st_mode))
            return 0 ;
    }
    else if (S_ISFIFO(st.st_mode)) {
        if (mkfifo(dst->s, st.st_mode) < 0)
            return 0 ;
    }
    else if (S_ISLNK(st.st_mode)) {
        size_t bufsz = st.st_size > 0 ? (size_t)st.st_size + 1 : 4096 ;
        char target[bufsz] ;
        ssize_t n = readlink(src->s, target, bufsz - 1) ;
        if (n < 0)
            return 0 ;
        target[n] = 0 ;
        if (symlink(target, dst->s) < 0) {
            /* Idempotent like file_copy (O_TRUNC) and dircopy (EEXIST tolerated):
             * an already-present destination must not fail. Replace it, but let
             * any other error (e.g. EISDIR on unlink, EACCES) stay fatal. */
            if (errno != EEXIST) return 0 ;
            if (unlink(dst->s) < 0) return 0 ;
            if (symlink(target, dst->s) < 0) return 0 ;
        }
    }
    else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISSOCK(st.st_mode)) {
        if (mknod(dst->s, st.st_mode, st.st_rdev) < 0)
            return 0 ;
    }
    else
        return (errno = ENOTSUP, 0) ;

    lchown(dst->s, st.st_uid, st.st_gid) ;   /* best-effort, errors ignored */
    if (!S_ISLNK(st.st_mode))
        chmod(dst->s, st.st_mode) ;
    return 1 ;
}

int tree_copy(char const *src, char const *dst)
{
    _alloc_strbuf_(s, 256) ;
    _alloc_strbuf_(d, 256) ;
    if (!strbuf_cats(&s, src) || !term(&s))
        return 0 ;
    if (!strbuf_cats(&d, dst) || !term(&d))
        return 0 ;
    return copy_node(&s, &d) ;
}
