/*
 * spawn_path_merge.c
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
 */

#include <stddef.h>
#include <sys/types.h>

#include <oblibs/spawn.h>

/** spawn_path_full owns the environ_merge (the merged VLA must stay in scope
 * across posix_spawnp); this is the merge-only wrapper, no flags/file-actions. */
pid_t spawn_path_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen)
{
    return spawn_path_full(file, argv, envp, modif, modiflen, 0, 0, 0) ;
}
