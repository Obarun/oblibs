/*
 * exec_path.c
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
 *
 * PATH resolution logic adapted from skalibs libposixplz execvep /
 * execvep_internal (ISC license, Laurent Bercot). Rewritten for oblibs to
 * walk a null-terminated PATH with str_search instead of a length-tracked
 * byte_chr, and folded into a single translation unit.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <oblibs/exec.h>
#include <oblibs/string.h>

/* Try every ':'-separated directory of @path in turn. @path is
 * null-terminated. Returns only on failure; errno reflects the most
 * meaningful failure encountered (see the skalibs rationale: ENOENT on a
 * given directory is not fatal to the search, the harder errors are). */
static void exec_search (char const *file, char const *const *argv, char const *const *envp, char const *path)
{
    size_t filelen = strlen(file) ;
    int e = 0 ;

    for (;;) {

        size_t split = str_search(path, ':') ;

        if (split) {

            char tmp[split + 2 + filelen] ;
            memcpy(tmp, path, split) ;
            tmp[split] = '/' ;
            memcpy(tmp + split + 1, file, filelen + 1) ;
            execve(tmp, (char *const *)argv, (char *const *)envp) ;

            if (errno != ENOENT) {
                e = errno ;
                if (errno != EACCES && errno != EPERM && errno != EISDIR && errno != ENOTDIR)
                    break ;
            }
        }

        if (!path[split])
            break ;

        path += split + 1 ;
    }

    if (e)
        errno = e ;
}

void exec_path (char const *file, char const *const *argv, char const *const *envp)
{
    if (strchr(file, '/')) {

        execve(file, (char *const *)argv, (char *const *)envp) ;

    } else {

        char const *path = getenv("PATH") ;
        if (!path) path = EXEC_DEFAULTPATH ;
        exec_search(file, argv, envp, path) ;
    }
}
