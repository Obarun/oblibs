/*
 * spawn_path_full.c
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
 * The low-level posix_spawn(p) primitive behind spawn_path / spawn_path_merge.
 * Same contract as spawn_path -- posix_spawn alone, no fork() fallback, never
 * wait()/reap, empty child sigmask -- extended with optional environment merge,
 * spawn flags (SETSID), and child file-actions. Still posix_spawn-only: no
 * capability here needs clone()/fork (NEWPIDNS stays out, by design).
 */

#include <spawn.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#include <oblibs/spawn.h>
#include <oblibs/exec.h>
#include <oblibs/environ.h>

/** Translate one spawn_fa_t into the matching posix_spawn_file_actions_* calls.
 * Returns 0 on success, or the errno-style error code on failure. */
static int spawn_fa_add (posix_spawn_file_actions_t *actions, spawn_fa_t const *a)
{
    switch (a->type) {
        case SPAWN_FA_CLOSE :
            return posix_spawn_file_actions_addclose(actions, a->from) ;
        case SPAWN_FA_DUP2 :
            return posix_spawn_file_actions_adddup2(actions, a->from, a->to) ;
        case SPAWN_FA_MOVE : {
            int e = posix_spawn_file_actions_adddup2(actions, a->from, a->to) ;
            /** Skip the close when from == to: it would close the fd we keep.
             * (posix_spawn has no action to clear O_CLOEXEC -- documented.) */
            if (!e && a->from != a->to)
                e = posix_spawn_file_actions_addclose(actions, a->from) ;
            return e ;
        }
        case SPAWN_FA_OPEN :
            return posix_spawn_file_actions_addopen(actions, a->to, a->path, a->oflag, a->mode) ;
        default :
            return EINVAL ;
    }
}

pid_t spawn_path_full (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen, uint16_t flags, spawn_fa_t const *fa, size_t nfa)
{
    posix_spawnattr_t attr ;
    posix_spawn_file_actions_t actions ;
    sigset_t set ;
    pid_t pid ;
    int e ;

    /** Reject unknown flag bits instead of silently dropping them: a caller
     * asking for a capability we do not implement must fail, not succeed with
     * its request quietly ignored. */
    if (flags & ~(uint16_t)SPAWN_FLAG_SETSID)
        return (errno = EINVAL, 0) ;

    /** Child starts with no signal blocked: it must not inherit a mask the
     * caller may have set to drive a signalfd. SETSID, when asked, ORs in. */
    e = posix_spawnattr_init(&attr) ;
    if (e)
        return (errno = e, 0) ;

    sigemptyset(&set) ;

    e = posix_spawnattr_setsigmask(&attr, &set) ;
    if (!e) {
        short spawnflags = POSIX_SPAWN_SETSIGMASK ;
        if (flags & SPAWN_FLAG_SETSID)
            spawnflags |= POSIX_SPAWN_SETSID ;
        e = posix_spawnattr_setflags(&attr, spawnflags) ;
    }
    if (e) {
        posix_spawnattr_destroy(&attr) ;
        return (errno = e, 0) ;
    }

    e = posix_spawn_file_actions_init(&actions) ;
    if (e) {
        posix_spawnattr_destroy(&attr) ;
        return (errno = e, 0) ;
    }
    for (size_t i = 0 ; i < nfa ; i++) {
        e = spawn_fa_add(&actions, fa + i) ;
        if (e) goto err ;
    }

    /** Optional environment merge (spawn_path_merge's job, folded in here so the
     * merged VLA stays in scope across posix_spawnp). When modif is NULL the
     * child env is envp verbatim. */
    {
        size_t envlen = modif ? environ_length(envp) : 0 ;
        size_t modifn = 0 ;
        if (modif) {
            for (size_t pos = 0 ; pos < modiflen ; pos++)
                if (!modif[pos]) modifn++ ;
        }

        /** Sized for the merge when modif is set; just 1 element otherwise, so
         * the no-merge path (spawn_path) keeps its near-zero stack footprint. */
        char const *merged[modif ? envlen + modifn + 1 : 1] ;
        char const *const *realenvp = envp ;
        if (modif) {
            environ_merge(merged, envlen + modifn + 1, envp, envlen, modif, modiflen) ;
            realenvp = merged ;
        }

        /** posix_spawnp searches the caller's environment PATH; force
         * EXEC_DEFAULTPATH when unset, matching exec_path(). The environ touch
         * is transient and only happens in the rare PATH-unset case. */
        int nopath = !getenv("PATH") ;
        if (nopath && setenv("PATH", EXEC_DEFAULTPATH, 0) == -1) {
            e = errno ;
            goto err ;
        }
        e = posix_spawnp(&pid, file, &actions, &attr, (char *const *)argv, (char *const *)realenvp) ;
        if (nopath)
            unsetenv("PATH") ;
    }

    posix_spawn_file_actions_destroy(&actions) ;
    posix_spawnattr_destroy(&attr) ;

    return e ? (errno = e, 0) : pid ;

 err :
    posix_spawn_file_actions_destroy(&actions) ;
    posix_spawnattr_destroy(&attr) ;
    return (errno = e, 0) ;
}
