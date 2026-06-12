/*
 * process_wait.c
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

#include <sys/wait.h>
#include <errno.h>

#include <oblibs/process.h>

pid_t process_wait (pid_t pid, int *wstat)
{
    pid_t r ;
    do r = waitpid(pid, wstat, 0) ;
    while (r == -1 && errno == EINTR) ;
    return r ;
}
