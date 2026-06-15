/*
 * spawn_path.c
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
 * posix_spawn(p)-only implementation: no fork() fallback, no "early return"
 * waitid workaround. We never wait()/reap the child here; the caller owns its
 * lifecycle. The empty sigmask is the one piece of child setup we force, for
 * the signalfd-using supervision case. The real work lives in spawn_path_full;
 * this is the no-merge / no-flags / no-file-actions wrapper.
 */

#include <sys/types.h>

#include <oblibs/spawn.h>

pid_t spawn_path (char const *file, char const *const *argv, char const *const *envp)
{
    return spawn_path_full(file, argv, envp, 0, 0, 0, 0, 0) ;
}
