/*
 * spawn_path_merge.c
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

#include <stddef.h>
#include <sys/types.h>

#include <oblibs/spawn.h>
#include <oblibs/environ.h>

pid_t spawn_path_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen)
{
    size_t envlen = environ_length(envp) ;

    /* Count modif entries (null-terminated records): worst case every modif is
     * an addition, so size the array for envlen + modifn variables + NULL. */
    size_t modifn = 0, pos = 0 ;
    for (; pos < modiflen ; pos++)
        if (!modif[pos]) modifn++ ;

    char const *newenvp[envlen + modifn + 1] ;
    environ_merge(newenvp, envlen + modifn + 1, envp, envlen, modif, modiflen) ;

    /* The VLA stays valid throughout: posix_spawn is synchronous w.r.t. envp
     * consumption on the supported libcs (glibc/musl/BSD/Darwin suspend the
     * caller until the child has exec'd), so spawn_path does not return before
     * newenvp has been read. */
    return spawn_path(file, argv, newenvp) ;
}
