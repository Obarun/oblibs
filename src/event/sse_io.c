/*
 * sse_io.c
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

#include <oblibs/log.h>

#include <oblibs/sse.h>

int sse_start_io(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int fd, int event, int priority)
{
    if (!w || !p || fd < 0) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "io watcher is NULL") ;
    }

    struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
    if (h != NULL)
        return 1 ;

    if (!sse_watcher_init(p, w, SSE_TYPE_IO, cb, cbdata, fd, event, priority))
        log_warnusys_return(LOG_EXIT_ZERO, "initiate io watcher") ;

    return sse_watcher_add(w) ;
}

int sse_modify_io(sse_watcher_t *w, int fd, int event)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "io watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p || fd < 0) {
        sse_err_return(w, EINVAL) ;
    }

    struct sse_watcher_hash_s *h = sse_hash_search(w->p, w) ;
    if (h == NULL) { // something really wrong here
        log_warnu("find io watchers is hash list") ;
        sse_err_return(w, EINVAL) ;
    }

    bool active = sse_watcher_active(w) ;
    int fd_changed = (w->fd != fd) ;

    /* Changing the fd is not a MOD: the new fd is not registered in epoll yet
     * (EPOLL_CTL_MOD would fail with ENOENT, silently tolerated) and the old fd
     * would stay armed. Drop the old fd from epoll, switch, then re-add under
     * the new fd. A pure event change keeps the same fd and is a genuine MOD. */
    if (active && fd_changed) {
        if (!sse_watcher_stop(w))      // EPOLL_CTL_DEL on the old fd, active=false
            return 0 ;
        w->fd = fd ;
        w->events = event ;
        return sse_watcher_restart(w) ; // inactive now -> just EPOLL_CTL_ADD the new fd
    }

    w->fd = fd ;
    w->events = event ;

    if (active)
        return sse_watcher_modify(w) ;

    return 1 ;
}

int sse_restart_io(sse_watcher_t *w)
{
    return sse_watcher_restart(w) ;
}

int sse_stop_io(sse_watcher_t *w)
{
    return sse_watcher_stop(w) ;
}

int sse_free_io(sse_watcher_t *w)
{
    return sse_watcher_free(w) ;
}