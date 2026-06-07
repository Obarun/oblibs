/*
 * sse_signal.c
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
#include <signal.h>

#include <oblibs/linux.h>
#include <oblibs/log.h>

#include <oblibs/sse.h>

int sse_start_signal(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int priority)
{
    if (!w || !p) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "signal watcher is NULL") ;
    }

	struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
	if (h != NULL)
		return 1 ;

    int fd = lx_signalfd_init() ;
	if (fd < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "initiate signalfd") ;


    if (!sse_watcher_init(p, w, SSE_TYPE_SIGNAL, cb, cbdata, fd, SSE_READ, priority)) {
        lx_signalfd_end() ;
        log_warnusys_return(LOG_EXIT_ZERO, "initiate signalfd watcher") ;
    }

    w->sdata = malloc(sizeof(sse_signal_t)) ;
    if (w->sdata == NULL) {
        close(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "malloc signalfd struct watcher") ;
    }
    memset(w->sdata, 0, sizeof(sse_signal_t)) ;

    return sse_watcher_add(w) ;
}

int sse_attach_signal(sse_watcher_t *w, int signal)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p) {
        sse_err_return(w, EINVAL) ;
    }

	if (!lx_signalfd_add(signal)) {
        log_warnusys("add signal: ", strsignal(signal)) ;
        sse_err_return(w, errno) ;
    }

    return 1 ;
}

int sse_detach_signal(sse_watcher_t *w, int signal)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p) {
        sse_err_return(w, EINVAL) ;
    }

    return lx_signalfd_del(signal) ;
}

int sse_ignore_signal(sse_watcher_t *w, int signal)
{

    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p) {
        sse_err_return(w, EINVAL) ;
    }

    return lx_signalfd_ignore(signal) ;
}

int sse_restart_signal(sse_watcher_t *w)
{
	return sse_watcher_restart(w) ;
}

int sse_stop_signal(sse_watcher_t *w)
{
	return sse_watcher_stop(w) ;
}

int sse_free_signal(sse_watcher_t *w)
{
	lx_signalfd_end() ;
    if (w->fd >= 0)
        close(w->fd) ;
    w->fd = -1 ;
	return sse_watcher_free(w) ; ;
}