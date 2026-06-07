/*
 * sse_schedule.c
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
#include <sys/timerfd.h>
#include <time.h>

#include <oblibs/log.h>

#include <oblibs/sse.h>

static bool populate_timeout(sse_watcher_t *w, time_t when)
{
    if (!w || !w->sdata || when < 0)
        return false ;

    sse_schedule_t *s = w->sdata ;
    s->when.it_value.tv_sec = when ;
    s->when.it_value.tv_nsec = 0 ;
    s->when.it_interval.tv_sec = 0 ;
    s->when.it_interval.tv_nsec = 0 ;

    return true ;
}


int sse_start_schedule(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, cron_t *expr, int priority)
{
    if (!w || !p || !expr) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "schedule watcher is NULL") ;
    }

	struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
	if (h != NULL)
		return 1 ;

	int fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC) ;
	if (fd < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "timerfd_create") ;

    if (!sse_watcher_init(p, w, SSE_TYPE_SCHEDULE, cb, cbdata, fd, SSE_READ, priority)) {
		close(fd) ;
		log_warnusys_return(LOG_EXIT_ZERO, "initiate schedule watcher") ;
    }

    w->sdata = malloc(sizeof(sse_schedule_t)) ;
    if (w->sdata == NULL) {
        close(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "malloc schedule watcher") ;
    }
    memset(w->sdata, 0, sizeof(sse_schedule_t)) ;

    ((sse_schedule_t *)w->sdata)->expr = expr ;
    ((sse_schedule_t *)w->sdata)->clockchange = false ;

    time_t when = sse_getfire_schedule(w) ;

	if (when < 0 || !populate_timeout(w, when)) {
        free(w->sdata) ;
        w->sdata = NULL ;
        close(fd) ;
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "invalid schedule time period") ;
    }

	if (timerfd_settime(w->fd, TFD_SETTIME_FLAGS, &((sse_schedule_t *)w->sdata)->when, NULL) < 0) {
        int e = errno ;
        free(w->sdata) ;
        w->sdata = NULL ;
        close(fd) ;
        errno = e ;
        log_warnusys_return(LOG_EXIT_ZERO, "timerfd_settime") ;
    }

	return sse_watcher_add(w) ;
}

int sse_modify_schedule(sse_watcher_t *w, time_t when)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "schedule watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p || when < 0 || w->type != SSE_TYPE_SCHEDULE) {
        sse_err_return(w, EINVAL) ;
    }

    if (!populate_timeout(w, when)) {
        sse_err_return(w, EINVAL) ;
    }

    if (sse_watcher_active(w)) {

        if (timerfd_settime(w->fd, TFD_SETTIME_FLAGS, &((sse_schedule_t *)w->sdata)->when, NULL) < 0) {
            int e = errno ;
            sse_stop_schedule(w) ;
            sse_err_return(w, e) ;
        }

        return sse_watcher_modify(w) ;
    }

    return 1 ;
}

int sse_restart_schedule(sse_watcher_t *w)
{
	if (!sse_watcher_restart(w))
        sse_err_return(w, w->api_errno) ;

    if (timerfd_settime(w->fd, TFD_SETTIME_FLAGS, &((sse_schedule_t *)w->sdata)->when, NULL) < 0) {
        int e = errno ;
        sse_stop_schedule(w) ;
        sse_err_return(w, e) ;
    }

    return sse_watcher_modify(w) ;
}

int sse_stop_schedule(sse_watcher_t *w)
{
	return sse_stop_timer(w);
}

int sse_free_schedule(sse_watcher_t *w)
{
    return sse_watcher_free(w);
}

time_t sse_getfire_schedule(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "schedule watcher is NULL") ;
    }

    struct timespec now ;
    if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
        int e = errno ;
        sse_stop_schedule(w) ;
        sse_err_return(w, e) ;
    }

    time_t when = cron_next(((sse_schedule_t *)w->sdata)->expr, &now.tv_sec) ;

    return when ;
}