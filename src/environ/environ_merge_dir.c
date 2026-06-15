/*
 * environ_merge_dir.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/environ.h>

uint8_t environ_merge_dir(strbuf *modifs, char const *path)
{
    int r ;
    size_t pos = 0, plen = strlen(path) ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char const *exclude[1] = { 0 } ;

    r = sbl_dir_get(&sa,path,exclude,S_IFREG) ;
    if (!r)
        return 0 ;

    /** empty path directory
     * we don't die */
    if (!sa.len)
        return 1 ;

    if (!sbl_sort(&sa))
        return 0 ;

    if (sbl_count(&sa) > MAXFILE) {
        log_warn("too many files to parse from: ",path) ;
        errno = E2BIG ;
        return 0 ;
    }

    FOREACH_SBL(&sa, pos) {

        _alloc_strbuf_(spath, plen + (strlen(sa.s + pos)) + 2) ;

        if (!strbuf_catb(&spath, path, plen) ||
            !strbuf_catb(&spath, "/", 1) ||
            !strbuf_catb(&spath, sa.s + pos, strlen(sa.s + pos)) ||
            !strbuf_terminate(&spath))
                return 0 ;

        if (!environ_merge_file(modifs, spath.s))
            return 0 ;
    }

    if (!strbuf_terminate(modifs))
        return 0 ;

    modifs->len-- ;

    return 1 ;
}

uint8_t environ_merge_dir_g(strbuf *modifs, const char *path)
{
    if (!environ_merge_dir(modifs, path) ||
        !environ_clean_unexport(modifs))
            return 0 ;

    return 1 ;
}
