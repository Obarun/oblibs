/*
 * sse_child.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/syscall.h> // For syscall(SYS_pidfd_open)
#include <unistd.h>
#include <signal.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>

// (syscall for portability)
static int get_pidfd(pid_t pid)
{
    return syscall(SYS_pidfd_open, pid, 0) ;
}

int sse_start_child(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, pid_t pid, int priority, bool sigchild)
{
    if (!w || !p || pid <= 0) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "child watcher is NULL") ;
    }

    sigset_t set, oldset ;
    struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
    if (h != NULL)
        return 1 ;

    // Block SIGCHLD if requested
    if (sigchild) {
        sigemptyset(&set) ;
        sigaddset(&set, SIGCHLD) ;
        if (sigprocmask(SIG_BLOCK, &set, &oldset) < 0)
            return 0 ;
    }

    int fd = get_pidfd(pid) ;
    if (fd < 0) {
        if (sigchild)
            sigprocmask(SIG_SETMASK, &oldset, NULL) ;
        log_warnusys_return(LOG_EXIT_ZERO, "get_pidfd") ;
    }


    if (!sse_watcher_init(p, w, SSE_TYPE_CHILD, cb, cbdata, fd, SSE_READ, priority)) {
        close(fd) ;
        if (sigchild)
            sigprocmask(SIG_SETMASK, &oldset, NULL) ;
        log_warnusys_return(LOG_EXIT_ZERO, "initiate child watcher") ;
    }

    w->sdata = malloc(sizeof(sse_child_t)) ;
    if (w->sdata == NULL) {
        close(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "malloc child struct watcher") ;
    }
    memset(w->sdata, 0, sizeof(sse_child_t)) ;

    ((sse_child_t *)w->sdata)->pid = pid ;
    ((sse_child_t *)w->sdata)->status = 0 ;

    int r = sse_watcher_add(w) ;

    // Restore mask on success
    if (sigchild)
        sigprocmask(SIG_SETMASK, &oldset, NULL) ;

    return r ;
}

int sse_restart_child(sse_watcher_t *w)
{
    return sse_watcher_restart(w) ;
}

int sse_stop_child(sse_watcher_t *w)
{
    return sse_watcher_stop(w) ;
}

int sse_free_child(sse_watcher_t *w)
{
    return sse_watcher_free(w) ;
}