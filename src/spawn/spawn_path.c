/*
 * spawn_path.c
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
 * posix_spawn(p)-only implementation: no fork() fallback, no "early return"
 * waitid workaround. We never wait()/reap the child here; the caller owns its
 * lifecycle. The empty sigmask is the one piece of child setup we force, for
 * the signalfd-using supervision case.
 */

#include <spawn.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

#include <oblibs/spawn.h>
#include <oblibs/exec.h>

pid_t spawn_path (char const *file, char const *const *argv, char const *const *envp)
{
    posix_spawnattr_t attr ;
    sigset_t set ;
    pid_t pid ;
    int e ;

    /* Child starts with no signal blocked: it must not inherit a mask the
     * caller may have set to drive a signalfd. */
    e = posix_spawnattr_init(&attr) ;
    if (e) return (errno = e, 0) ;
    sigemptyset(&set) ;
    e = posix_spawnattr_setsigmask(&attr, &set) ;
    if (!e) e = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK) ;
    if (e) {
        posix_spawnattr_destroy(&attr) ;
        return (errno = e, 0) ;
    }

    /* posix_spawnp searches the caller's environment PATH; force EXEC_DEFAULTPATH
     * when unset, matching exec_path(). The environ touch is transient and only
     * happens in the rare PATH-unset case. */
    {
        int nopath = !getenv("PATH") ;
        if (nopath && setenv("PATH", EXEC_DEFAULTPATH, 0) == -1) {
            e = errno ;
            posix_spawnattr_destroy(&attr) ;
            return (errno = e, 0) ;
        }
        e = posix_spawnp(&pid, file, NULL, &attr, (char *const *)argv, (char *const *)envp) ;
        if (nopath) unsetenv("PATH") ;
    }

    posix_spawnattr_destroy(&attr) ;

    return e ? (errno = e, 0) : pid ;
}
