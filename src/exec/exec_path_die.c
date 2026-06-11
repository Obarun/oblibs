/*
 * exec_path_die.c
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

#include <errno.h>

#include <oblibs/exec.h>
#include <oblibs/log.h>

void exec_path_die (char const *file, char const *const *argv, char const *const *envp)
{
    exec_path(file, argv, envp) ;
    log_dieusys(errno == ENOENT ? 127 : 126, "exec ", file) ;
}
