/*
 * sbl_dir.c
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

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/sbl.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

static inline int dot_or_dotdot(char const *name)
{
    if (name[0] == '.')
        if (((name[1] == '.') && !name[2]) || !name[1])
            return 1 ;
    return 0 ;
}

int sbl_dir_get(strbuf *sb, char const *srcdir, char const **exclude, uint32_t flags)
{
    size_t base = sb->len ;
    int wasnull = !sb->s ;

    if (!flags || flags & ~FLAGS_ALL)
        return (errno = EINVAL, 0) ;

    DIR *dir = opendir(srcdir) ;
    if (!dir)
        return 0 ;

    int fdsrc = dirfd(dir) ;

    for (;;) {
        struct stat st ;
        struct dirent *d = readdir(dir) ;
        if (!d) break ;

        if (dot_or_dotdot(d->d_name))
            continue ;

        if (fstatat(fdsrc, d->d_name, &st, 0) < 0)
            goto err ;

        if (mode_to_flags(st.st_mode) & flags) {
            char const *const *s = exclude ;
            while (*s)
                if (!strcmp(d->d_name, *s++))
                    goto next ;
            if (!strbuf_catb(sb, d->d_name, strlen(d->d_name) + 1))
                goto err ;
        }
        next: ;
    }
    dir_close(dir) ;
    return 1 ;
    err:
        dir_close(dir) ;
        if (wasnull) strbuf_free(sb) ;
        else sb->len = base ;
        return 0 ;
}

int sbl_dir_get_recursive(strbuf *sb, char const *srcdir, char const **exclude, uint32_t flags, uint8_t include_path)
{
    size_t base = sb->len ;
    int wasnull = !sb->s ;
    /* Pure heap (no _alloc_sbl_ stack seed) ON PURPOSE: this function recurses,
     * and a stack seed lives on its frame for the whole call, so seeds would
     * stack up as n_bytes * depth and could overflow the C stack on deep trees.
     * The malloc cost is negligible next to opendir/readdir anyway. See the
     * _alloc_strbuf_ note in strbuf.h. */
    _cleanup_strbuf_ strbuf subdir = STRBUF_ZERO ;

    if (!flags || flags & ~FLAGS_ALL)
        return (errno = EINVAL, 0) ;

    DIR *dir = opendir(srcdir) ;
    if (!dir)
        return 0 ;

    int fdsrc = dirfd(dir) ;

    for (;;) {
        struct stat st ;
        struct dirent *d = readdir(dir) ;
        if (!d) break ;

        if (dot_or_dotdot(d->d_name))
            continue ;

        if (fstatat(fdsrc, d->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0)
            goto err ;

        char const *const *s = exclude ;
        while (*s)
            if (!strcmp(d->d_name, *s++))
                goto next ;

        if (S_ISDIR(st.st_mode)) {
            subdir.len = 0 ;
            if (!auto_strbuf(&subdir, srcdir, "/", d->d_name))
                goto err ;
            if (!sbl_dir_get_recursive(sb, subdir.s, exclude, flags, include_path))
                goto err ;
        }

        if (mode_to_flags(st.st_mode) & flags) {
            if (include_path) {
                if (!strbuf_cats(sb, srcdir) ||
                    !strbuf_cats(sb, "/"))
                    goto err ;
            }
            if (!strbuf_catb(sb, d->d_name, strlen(d->d_name) + 1))
                goto err ;
        }
        next: ;
    }
    dir_close(dir) ;
    return 1 ;
    err:
        dir_close(dir) ;
        if (wasnull) strbuf_free(sb) ;
        else sb->len = base ;
        return 0 ;
}

int sbl_dir_get_element(strbuf *sb, char const *srcdir, char const *element, uint32_t flags, uint8_t include_path)
{
    size_t base = sb->len ;
    int wasnull = !sb->s ;
    int found = 0 ;

    if (!flags || flags & ~FLAGS_ALL)
        return (errno = EINVAL, -1) ;

    DIR *dir = opendir(srcdir) ;
    if (!dir)
        return -1 ;

    int fdsrc = dirfd(dir) ;

    for (;;) {
        struct stat st ;
        struct dirent *d = readdir(dir) ;
        if (!d) break ;

        if (dot_or_dotdot(d->d_name))
            continue ;

        if (fstatat(fdsrc, d->d_name, &st, 0) < 0)
            goto err ;

        if (mode_to_flags(st.st_mode) & flags) {
            if (!strcmp(d->d_name, element)) {
                if (include_path) {
                    if (!strbuf_cats(sb, srcdir) ||
                        !strbuf_cats(sb, "/"))
                        goto err ;
                }
                if (!strbuf_catb(sb, d->d_name, strlen(d->d_name) + 1))
                    goto err ;
                found++ ;
            }
        }
    }
    dir_close(dir) ;
    return found ;
    err:
        dir_close(dir) ;
        if (wasnull) strbuf_free(sb) ;
        else sb->len = base ;
        return -1 ;
}

int sbl_dir_drop_child(strbuf *sb)
{
    /* result only drops elements, so it fits in sb->len; non-recursive */
    _alloc_sbl_(tmp, sb->len) ;
    size_t pos = 0 ;
    uint8_t on_parent = 0 ;
    char *current = 0, *previous = 0 ;

    FOREACH_SBL(sb, pos) {
        current = sb->s + pos ;

        if (!strcmp(current, "/"))
            continue ;

        if (!previous) {
            if (!sbl_add(&tmp, current))
                return 0 ;
            previous = current ;
            continue ;
        }

        if (!dir_is_child(previous, current)) {
            if (!sbl_add(&tmp, current))
                return 0 ;
            on_parent = 1 ;
        }
        else on_parent = 0 ;

        if (on_parent)
            previous = current ;
    }

    return strbuf_copy(sb, &tmp) ;
}
