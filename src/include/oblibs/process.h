/*
 * process.h
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
 * Blocking process-lifecycle helpers, the counterpart of oblibs/spawn.h: spawn.h
 * deliberately never wait()s/reaps a child (the caller drives the lifecycle, e.g.
 * through a pidfd watched by an sse loop). This module provides the simple
 * synchronous case -- block until a known child is reaped -- for callers that
 * have nothing else to do meanwhile.
 */

#ifndef OB_PROCESS_H
#define OB_PROCESS_H

#include <sys/types.h>

/** Wait for child @pid to terminate, retrying transparently on EINTR; stores the
 * wait status in *@wstat. Replaces the skalibs wait_pid macro (waitpid_nointr).
 * @Return the reaped pid on success, or -1 with errno set on failure. */
extern pid_t process_wait (pid_t pid, int *wstat) ;

#endif
