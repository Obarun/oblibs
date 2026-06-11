/*
 * spawn.h
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

#endif
