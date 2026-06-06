/*
 * dir_destroy.c
 *
 * Copyright (c) 2018-2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <oblibs/directory.h>
#include <oblibs/strbuf.h>

/* Ensure path->s is NUL-terminated at index len (the NUL is not counted in
 * len). strbuf does not zero its slack, so we write it explicitly. */
static int term(strbuf *p)
{
    if (!strbuf_reserve(p, p->len + 1))
        return 0 ;
    p->s[p->len] = 0 ;
    return 1 ;
}

/* Recursively remove the path currently held (NUL-terminated) in `path`,
 * reusing it as scratch for child paths. Returns 0 on success, -1 on error
 * (errno set). Mirrors skalibs rm_rf_in_tmp: try unlink first; for a directory
 * (EISDIR/EPERM) slurp the whole listing and CLOSE the directory before
 * recursing -- no DIR* stays open across recursion -- then rmdir. */
static int destroy(strbuf *path)
{
    if (unlink(path->s) == 0 || errno == ENOENT)
        return 0 ;
    if (errno != EISDIR && errno != EPERM)
        return -1 ;
    int isadir = errno == EPERM ;

    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;   /* entries, NUL-separated */
    DIR *d = opendir(path->s) ;
    if (!d) {
        if (isadir && errno == ENOTDIR) errno = EPERM ;
        return -1 ;
    }
    for (;;) {
        errno = 0 ;
        struct dirent *de = readdir(d) ;
        if (!de) break ;
        char const *nm = de->d_name ;
        if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0)))
            continue ;
        if (!strbuf_catb(&names, nm, strlen(nm) + 1)) { dir_close(d) ; return -1 ; }
    }
    if (errno) { dir_close(d) ; return -1 ; }   /* readdir failure */
    dir_close(d) ;

    size_t base = path->len ;
    for (size_t off = 0 ; off < names.len ; ) {
        char const *nm = names.s + off ;
        size_t nl = strlen(nm) ;
        path->len = base ;
        if (!strbuf_catb(path, "/", 1) || !strbuf_catb(path, nm, nl) || !term(path))
            return -1 ;
        if (destroy(path) == -1)
            return -1 ;
        off += nl + 1 ;
    }
    path->len = base ;
    if (!term(path))
        return -1 ;
    return rmdir(path->s) ;
}

int dir_destroy(char const *dir)
{
    _alloc_strbuf_(path, 256) ;
    if (!strbuf_cats(&path, dir) || !term(&path))
        return 0 ;
    return destroy(&path) < 0 ? 0 : 1 ;
}
