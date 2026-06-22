/*
 * exec_path_merge_die.c
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

#include <errno.h>

#include <oblibs/exec.h>
#include <oblibs/log.h>

void exec_path_merge_die (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen)
{
    exec_path_merge(file, argv, envp, modif, modiflen) ;
    log_dieusys(errno == ENOENT ? 127 : 126, "exec ", file) ;
}
