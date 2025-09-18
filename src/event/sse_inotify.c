/*
 * sse_inotify.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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

#include <oblibs/linux.h>
#include <oblibs/log.h>

#include <oblibs/sse.h>

int sse_start_inotify(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int priority)
{
    if (!w || !p) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "inotify watcher is NULL") ;
    }

    struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
    if (h != NULL)
        return 1 ;

    int fd = lx_inotify_init() ;
    if (fd < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "initiate inotify") ;

    if (!sse_watcher_init(p, w, SSE_TYPE_INOTIFY, cb, cbdata, fd, SSE_READ, priority)) {
        lx_inotify_end() ;
        log_warnusys_return(LOG_EXIT_ZERO, "initiate inotify watcher") ;
    }

    return sse_watcher_add(w) ;
}

int sse_attach_inotify(sse_watcher_t *w, const char *what, uint32_t imask)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "inotify watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p || !what) {
        sse_err_return(w, EINVAL) ;
    }

    if (!lx_inotify_add(what, imask)) {
        log_warnusys("add inotify path: ", what) ;
        sse_err_return(w, errno) ;
    }

    return 1 ;
}

int sse_detach_inotify(sse_watcher_t *w, const char *what)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "inotify watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p || !what) {
        sse_err_return(w, EINVAL) ;
    }

    return lx_inotify_del(what) ;
}

int sse_update_inotify(sse_watcher_t *w, const char *what, uint32_t imask)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "inotify watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p || !what) {
        sse_err_return(w, EINVAL) ;
    }

    return lx_inotify_update(what, imask) ;
}

int sse_restart_inotify(sse_watcher_t *w)
{
	return sse_watcher_restart(w) ;
}

int sse_stop_inotify(sse_watcher_t *w)
{
	return sse_watcher_stop(w) ;
}

int sse_free_inotify(sse_watcher_t *w)
{
    lx_inotify_end() ;
    if (w->fd >= 0)
        close(w->fd) ;
    w->fd = -1 ;
    return sse_watcher_free(w) ;
}

