/*
 * environ_merge_dir.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>

uint8_t environ_merge_dir(stralloc *modifs, char const *path)
{
    int r ;
    size_t pos = 0, plen = strlen(path) ;
    _alloc_sa_(sa) ;
    char const *exclude[1] = { 0 } ;

    r = sastr_dir_get(&sa,path,exclude,S_IFREG) ;
    if (!r)
        return 0 ;

    /** empty path directory
     * we don't die */
    if (!sa.len)
        return 1 ;

    if (!sastr_sort(&sa))
        return 0 ;

    if (sastr_nelement(&sa) > MAXFILE) {
        log_warn("too many files to parse from: ",path) ;
        errno = E2BIG ;
        return 0 ;
    }

    FOREACH_SASTR(&sa, pos) {

        _alloc_stk_(spath, plen + (strlen(sa.s + pos)) + 2) ;

        if (!stack_add(&spath, path, plen) ||
            !stack_add(&spath, "/", 1) ||
            !stack_add(&spath, sa.s + pos, strlen(sa.s + pos)) ||
            !stack_close(&spath))
                return 0 ;

        if (!environ_merge_file(modifs, spath.s))
            return 0 ;
    }

    if (!stralloc_0(modifs))
        return 0 ;

    modifs->len-- ;

    return 1 ;
}

uint8_t environ_merge_dir_g(stralloc *modifs, const char *path)
{
    if (!environ_merge_dir(modifs, path) ||
        !environ_clean_unexport(modifs))
            return 0 ;

    return 1 ;
}
