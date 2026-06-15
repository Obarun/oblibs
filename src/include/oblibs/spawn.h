/*
 * spawn.h
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
 * Spawn a child process with posix_spawn. The "in a child" counterpart of
 * oblibs/exec.h: same PATH resolution, but instead of replacing the current
 * process image it creates a new process and returns its pid.
 *
 * Replaces the skalibs/cspawn.h family (cspawn / child_spawn0 / mspawn_*).
 * Unlike skalibs there is NO fork() fallback and NO posix_spawn "early return"
 * workaround: we rely on posix_spawn alone. The functions NEVER wait()/reap the
 * child -- the whole child lifecycle belongs to the caller (e.g. an SSE epoll
 * loop watching the pid through a pidfd). This is deliberate: an internal
 * wait()/reap would race with such monitoring.
 *
 * The child always starts with an EMPTY signal mask (all signals unblocked),
 * which is mandatory when the caller blocks signals to drive a signalfd: the
 * child must not inherit a blocked mask.
 */

#ifndef OB_SPAWN_H
#define OB_SPAWN_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

/** A single child file-action, applied (in array order) before exec, mapped
 * onto the matching posix_spawn_file_actions_* primitive:
 *   SPAWN_FA_CLOSE  close(from)
 *   SPAWN_FA_DUP2   dup2(from, to)               -- @from stays open
 *   SPAWN_FA_MOVE   dup2(from, to) + close(from) -- the close is skipped when
 *                   @from == @to (it would otherwise close the fd we keep).
 *                   Note: posix_spawn cannot clear O_CLOEXEC; in the @from==@to
 *                   case a CLOEXEC source fd is still closed at exec, so the
 *                   caller must keep it non-CLOEXEC there. The @from!=@to path
 *                   is safe (the duplicated @to does not inherit CLOEXEC).
 *   SPAWN_FA_OPEN   open(path, oflag, mode) onto fd @to */
enum spawn_fa_e {
    SPAWN_FA_CLOSE = 0,
    SPAWN_FA_DUP2,
    SPAWN_FA_MOVE,
    SPAWN_FA_OPEN
} ;

typedef struct spawn_fa_s spawn_fa_t ;
struct spawn_fa_s {
    uint8_t type ;       // enum spawn_fa_e
    int from ;           // CLOSE: fd closed; DUP2/MOVE: source fd; OPEN: unused
    int to ;             // DUP2/MOVE: destination fd; OPEN: resulting fd; CLOSE: unused
    char const *path ;   // OPEN only
    int oflag ;          // OPEN only
    unsigned int mode ;  // OPEN only (mode_t)
} ;

/** Set the child's session id (POSIX_SPAWN_SETSID): the child becomes a session
 * and process-group leader, so a later killpg() on its pid hits the child and
 * not the spawner. ORed onto the always-forced empty signal mask. */
#define SPAWN_FLAG_SETSID 0x01U
/** 0x02U reserved for a possible NEWPIDNS -- out of scope here: a new PID
 * namespace needs clone()/unshare(), which posix_spawn cannot do. */

/** Spawn @file, executed with @argv and @envp, via posix_spawn. If @file
 * contains a '/', it is executed verbatim; otherwise it is searched in the
 * PATH of the CURRENT environment (getenv("PATH"), falling back to
 * EXEC_DEFAULTPATH) -- the caller's PATH, independent of the @envp handed to
 * the executed program, exactly like exec_path().
 * The child starts with an empty signal mask. Never forks, never waits.
 * Returns the child pid on success, or 0 with errno set on failure. On the
 * mainstream libcs (glibc, musl, BSD, Darwin) a failing execve is reported
 * here; on an exotic "early-return" libc it instead surfaces at wait() time as
 * a child exit status of 127. */
extern pid_t spawn_path (char const *file, char const *const *argv, char const *const *envp) ;

/** Like spawn_path but, before spawning, merges the modif string @modif (of
 * length @modiflen, a sequence of null-terminated "KEY=value" to set / "KEY"
 * to unset) into @envp via environ_merge, exactly like exec_path_merge.
 * Returns the child pid on success, or 0 with errno set on failure. */
extern pid_t spawn_path_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen) ;

/** The low-level primitive both spawn_path and spawn_path_merge wrap: every
 * spawn parameter is explicit. PATH resolution and the empty child signal mask
 * behave exactly as in spawn_path. In addition:
 *  - @modif (of length @modiflen) is merged into @envp via environ_merge before
 *    the spawn, exactly like spawn_path_merge; pass @modif == 0 to skip merging.
 *  - @flags ORs SPAWN_FLAG_* bits onto the forced empty mask (e.g. SPAWN_FLAG_SETSID).
 *  - @fa is an array of @nfa file-actions applied to the child in order.
 * Returns the child pid on success, or 0 with errno set on failure (errno
 * EINVAL on an unknown file-action type or an unknown flag bit). Never forks,
 * never waits. */
extern pid_t spawn_path_full (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen, uint16_t flags, spawn_fa_t const *fa, size_t nfa) ;

#endif
