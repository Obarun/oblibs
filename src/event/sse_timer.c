/*
 * sse_timer.c
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
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/timerfd.h>
#include <time.h>

#include <oblibs/log.h>

#include <oblibs/sse.h>

// timeout and period in ms
static bool populate_timeout(sse_watcher_t *w, int timeout, int period)
{
    if (!w || !w->sdata)
        return false ;

    if (timeout < 0) {
        // dearm the timer, this will be used as an infinite timeout
        ((sse_timer_t *)w->sdata)->timeout.it_value.tv_sec = 0 ;
        ((sse_timer_t *)w->sdata)->timeout.it_value.tv_nsec = 0 ;
        ((sse_timer_t *)w->sdata)->timeout.it_interval.tv_sec = 0 ;
        ((sse_timer_t *)w->sdata)->timeout.it_interval.tv_nsec = 0 ;
        return true ;
    }

    int ftimeout = !timeout ? 1 : timeout ;

    ((sse_timer_t *)w->sdata)->timeout.it_value.tv_sec = ftimeout / 1000 ;
    ((sse_timer_t *)w->sdata)->timeout.it_value.tv_nsec = (ftimeout % 1000) * 1000000 ;
    ((sse_timer_t *)w->sdata)->timeout.it_interval.tv_sec = period <= 0 ? 0 : (period / 1000) ;
    ((sse_timer_t *)w->sdata)->timeout.it_interval.tv_nsec = (period <= 0 ? 0 : (period % 1000) * 1000000) ;

    return true ;
}

int sse_start_timer(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int timeout, int period, int priority)
{
    if (!w || !p) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
    if (h != NULL)
        return 1 ;

    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC) ;
	if (fd < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "timerfd_create") ;

	if (!sse_watcher_init(p, w, SSE_TYPE_TIMER, cb, cbdata, fd, SSE_READ, priority)) {
		close(fd) ;
		log_warnusys_return(LOG_EXIT_ZERO, "initiate timerfd watcher") ;
    }

    w->sdata = malloc(sizeof(sse_timer_t)) ;
    if (w->sdata == NULL) {
        close(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "malloc timerfd watcher") ;
    }

    memset(w->sdata, 0, sizeof(sse_timer_t)) ;

    if (!populate_timeout(w, timeout, period)) {
        free(w->sdata) ;
        w->sdata = NULL ;
        close(fd) ;
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "invalid timerfd period") ;
    }

    if (timerfd_settime(w->fd, 0, &((sse_timer_t *)w->sdata)->timeout, NULL) < 0) {
        int e = errno ;
        free(w->sdata) ;
        w->sdata = NULL ;
        close(fd) ;
        errno = e ;
        log_warnusys_return(LOG_EXIT_ZERO, "timerfd_settime") ;
    }

    return sse_watcher_add(w) ;
}

int sse_modify_timer(sse_watcher_t *w, int timeout, int period)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "timerfd watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p || timeout < 0 || w->type != SSE_TYPE_TIMER) {
        sse_err_return(w, EINVAL) ;
    }

    struct sse_watcher_hash_s *h = sse_hash_search(w->p, w) ;
    if (h == NULL) { // something really wrong here
        log_warnu("find timerfd watchers is hash list") ;
        sse_err_return(w, EINVAL) ;
    }

    if (!populate_timeout(w, timeout, period)) {
        sse_err_return(w, EINVAL) ;
    }

    if (sse_watcher_active(w)) {

        if (timerfd_settime(w->fd, 0, &((sse_timer_t *)w->sdata)->timeout, NULL) < 0) {
            int e = errno ;
            sse_stop_timer(w) ;
            sse_err_return(w, e) ;
        }

        return sse_watcher_modify(w) ;
    }

    return 1 ;
}

int sse_restart_timer(sse_watcher_t *w)
{
    if (!sse_watcher_restart(w))
        sse_err_return(w, w->api_errno) ;

    if (timerfd_settime(w->fd, 0, &((sse_timer_t *)w->sdata)->timeout, NULL) < 0) {
        int e = errno ;
        sse_stop_timer(w) ;
        sse_err_return(w, e) ;
    }

    return sse_watcher_modify(w) ;
}

int sse_stop_timer(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    struct itimerspec zero = { 0 } ;
    if (w->fd >= 0) // try to dearm the timer itself ;
        timerfd_settime(w->fd, 0, &zero, NULL) ;

    return sse_watcher_stop(w) ;
}

int sse_free_timer(sse_watcher_t *w)
{
    return sse_watcher_free(w) ;
}