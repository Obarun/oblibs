/*
 * exec_path_merge.c
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

#include <stddef.h>

#include <oblibs/exec.h>
#include <oblibs/environ.h>

void exec_path_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen)
{
    size_t envlen = environ_length(envp) ;

    /* Count modif entries (null-terminated records): worst case every modif is
     * an addition, so size the array for envlen + modifn variables + NULL. */
    size_t modifn = 0, pos = 0 ;
    for (; pos < modiflen ; pos++)
        if (!modif[pos]) modifn++ ;

    char const *newenvp[envlen + modifn + 1] ;
    environ_merge(newenvp, envlen + modifn + 1, envp, envlen, modif, modiflen) ;

    exec_path(file, argv, newenvp) ;
}
