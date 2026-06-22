/*
 * sse.c
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

#include <stdbool.h>    // For bool, true, false
#include <stdint.h>     // For uint32_t, uint64_t
#include <errno.h>      // For errno, EINVAL, EPERM, EBADF, EAGAIN
#include <string.h>     // For memset, strerror
#include <stdlib.h>     // For malloc, free, realloc
#include <sys/epoll.h>  // For epoll_create1, epoll_ctl, epoll_wait, struct epoll_event, EPOLL_CLOEXEC
#include <sys/select.h> // For select, fd_set, FD_ZERO, FD_SET
#include <sys/ioctl.h>  // For ioctl, FIONREAD
#include <time.h>       // For clock_gettime, struct timespec, CLOCK_MONOTONIC
#include <unistd.h>     // For close, read
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/queue.h>
#include <oblibs/hash.h>
#include <oblibs/linux.h>

#include <oblibs/sse.h>

struct sse_watcher_hash_s *sse_hash_search(sse_epoll_t *p, sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(NULL, "watcher is NULL") ;
    }

    struct sse_watcher_hash_s *h = NULL;
    h = hash_find(&p->watchers, &w, sizeof w) ;
    return h ;
}

int sse_hash_add(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    struct sse_watcher_hash_s *h = sse_hash_search(w->p, w) ;
    if (h != NULL) {
        log_warn_return(LOG_EXIT_ONE, "watcher already added to hash table") ;
    }

    h = (struct sse_watcher_hash_s *)malloc(sizeof(*h)) ;
    if (h == NULL) {
        log_warnu("allocate watcher") ;
        sse_err_return(w, ENOMEM) ;
    }

    memset(h, 0, sizeof(*h)) ;
    h->w = w ;
    hash_add(&w->p->watchers, &h->w, sizeof h->w, h) ;

    return 1 ;
} ;

void sse_hash_del(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("watcher is NULL") ;
        return ;
    }

    struct sse_watcher_hash_s *s = sse_hash_search(w->p, w) ;
    if (s == NULL) {
        log_warn("watcher already removed from hash table") ;
        return ;
    }

    if (w->sdata != NULL) {
        free(w->sdata) ;
        w->sdata = NULL ;
    }

    hash_del(&w->p->watchers, s) ;
    free(s) ;
    s = NULL ;
}

void sse_hash_free(hash_t *hash)
{
    struct sse_watcher_hash_s *c, *tmp ;
    HASH_FOREACH(hash, c, tmp) {
        sse_hash_del(c->w) ;
    }
    hash_free(hash) ;
}

int sse_epoll_add(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (w->p->fd > 0) {

        struct epoll_event e ;
        e.events = w->events | SSE_SHUT ;
        e.data.ptr = w ;

        if (epoll_ctl(w->p->fd, EPOLL_CTL_ADD, w->fd, &e) < 0) {

            if (errno == EEXIST) {
                log_warnsys("watcher already added to epoll") ;
                w->active = true ;
                return 1 ;
            }

            if (errno != EPERM || w->type != SSE_TYPE_IO || w->events != SSE_READ || w->fd) {
                log_warnusys("add watcher to epoll") ;
                sse_err_return(w, errno) ;
            }

            w->isfile = true ;
            if (!queue_insert_ptr(w->p->file, w)) {
                log_warnusys("add watcher to file list") ;
                sse_err_return(w, errno) ;
            }
        } else {
            w->active = true ;
        }
    }

    return 1 ;
}

int sse_epoll_del(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    w->active = false ;
    sse_err_zero(w) ;
    if (w->p->fd > 0) {
        // signalfd and inotify watcher may have already closed the fd
        if (w->fd >= 0) {
            if (epoll_ctl(w->p->fd, EPOLL_CTL_DEL, w->fd, NULL) < 0) {
                if (errno != ENOENT) {
                    log_warnusys("remove watcher from epoll") ;
                    sse_err_return(w, errno) ;
                }
            }
        }
    }
    return 1 ;
}

int sse_epoll_modify(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (w->p->fd > 0) {

        struct epoll_event e ;

        e.events = w->events | SSE_SHUT ;
        e.data.ptr = w ;

        if (epoll_ctl(w->p->fd, EPOLL_CTL_MOD, w->fd, &e) < 0) {
            if (errno != ENOENT) {
                log_warnusys("modify watcher to epoll") ;
                sse_err_return(w, errno) ;
            }
        }

        w->active = true ;
    }

    return 1 ;
}

static int is_stdin_event_persistent(int fd)
{
    if (fd < 0) {
        errno = EINVAL ;
        log_warnusys(LOG_EXIT_ZERO, "fd is not set") ;
    }

    struct timeval timeout = { 0, 0 };
    fd_set fds;
    int n = 0;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    if (select(1, &fds, NULL, NULL, &timeout) <= 0)
        log_warnusys_return(LOG_EXIT_ZERO, "select") ;

    if (ioctl(fd, FIONREAD, &n) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "ioctl") ;

    return n > 0 ;  // 1 if data, 0 if EOF
}

static void sse_watcher_file(sse_epoll_t *p)
{
    if (!p)
        return ;

    sse_watcher_t *w ;
    sse_watcher_t *remove[p->file->size + 1] ;
    size_t n = 0 ;

    p->rerun_file = false ;

    if (p->file->size) {

        FOREACH_QUEUE(p->file, w) {

            // double check to only deal with fd == 0, see sse_watcher_add
            if (w->fd)
                continue ;

            if (!is_stdin_event_persistent(w->fd))
                remove[n++] = w ;

            if (w->cb) {
                w->cb(w, w->cbdata, SSE_READ) ;
                p->rerun_file = true ;
            }
        }

        for (size_t i = 0 ; i < n ; i++){

            w = remove[i] ;

            if (!queue_remove_ptr(p->file, w))
                log_warnusys("remove watcher from file list") ;

            w->active = false ;
        }
    }
}

bool sse_watcher_active(sse_watcher_t *w)
{
	return !w ? false : w->active ;
}

int sse_watcher_init(sse_epoll_t *p, sse_watcher_t *w, sse_type_t type, sse_callback_t *cb, void *cbdata, int fd, int events, int priority)
{
    if (!w || !p) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    w->p = p ;
    w->type = type ;
    w->fd = fd ;
    w->events = events ;
    w->revents = 0 ;
    w->priority = priority ? priority : 0 ;
    w->active = false ;
    w->isfile = false ;

    w->cb = cb ;
    w->cbdata = cbdata ? cbdata : NULL ;
    w->sdata = NULL ;
    w->api_errno = 0 ;
    w->time.created.tv_sec = 0 ;
    w->time.created.tv_nsec = 0 ;
    w->time.dispatched.tv_sec = 0 ;
    w->time.dispatched.tv_nsec = 0 ;
    if (clock_gettime(CLOCK_MONOTONIC, &w->time.created) < 0) {
        log_warnusys("get time to set event created time") ;
        sse_err_return(w, errno) ;
    }

    return 1 ;
}

int sse_watcher_add(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (w->fd < 0 || !w->p) {
        sse_err_return(w, EINVAL) ;
    }

    if (!sse_epoll_add(w) && !sse_watcher_active(w)) {
        sse_err_return(w, errno) ;
    }

    if (!sse_hash_add(w)) {
        sse_err_return(w, w->api_errno) ;
    }

    return 1 ;
}

int sse_watcher_del(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!w->p) {
		sse_err_return(w, EINVAL) ;
    }

    if (w->isfile) {
        w->active = false ;
        return 1 ;
    }

    if (!sse_epoll_del(w)) {
        sse_err_return(w, w->api_errno) ;
    }

    sse_hash_del(w) ;

    return 1 ;
}

int sse_watcher_modify(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
	if (!w->p || w->fd < 0) {
		sse_err_return(w, EINVAL) ;
    }

    if (!sse_epoll_modify(w)) {
        sse_err_return(w, w->api_errno) ;
    }

	return 1 ;
}

int sse_watcher_restart(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
	if (w->fd < 0) {
        sse_err_return(w, EINVAL) ;
    }

	if (sse_watcher_active(w)) {
		if (!sse_epoll_del(w)) {
            sse_err_return(w, w->api_errno) ;
        }
    }

	if (!sse_epoll_add(w)) {
        sse_err_return(w, w->api_errno) ;
    }

	return 1 ;
}

int sse_watcher_stop(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
	if (w->fd < 0) {
		sse_err_return(w, EINVAL) ;
    }

	if (!sse_watcher_active(w))
		return 1 ;

    if (!sse_epoll_del(w)) {
		sse_err_return(w, w->api_errno) ;
    }

    return 1 ;
}

int sse_watcher_free(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "watcher is NULL") ;
    }

    sse_err_zero(w) ;
    if (!sse_watcher_del(w)) {
        sse_err_return(w, w->api_errno) ;
    }

    if (w->fd >= 0)
        close(w->fd) ;

    w->fd = -1 ;

    return 1 ;
}

int sse_watcher_cmp(const void *a, const void *b)
{
    const sse_watcher_t *wa = (sse_watcher_t *)a ;
    const sse_watcher_t *wb = (sse_watcher_t *)b ;
    if (wa->priority < wb->priority) return -1 ;
    if (wa->priority > wb->priority) return 1 ;
    if (wa->time.created.tv_sec < wb->time.created.tv_sec) return -1 ;
    if (wa->time.created.tv_sec > wb->time.created.tv_sec) return 1 ;
    if (wa->time.created.tv_nsec < wb->time.created.tv_nsec) return -1 ;
    if (wa->time.created.tv_nsec > wb->time.created.tv_nsec) return 1 ;

    return 0 ;
}

int sse_watcher_rmcmp(const void *a, const void *b)
{
    const sse_watcher_t *wa = (sse_watcher_t *)a ;
    const sse_watcher_t *wb = (sse_watcher_t *)b ;
    return (wa == wb) ? 0 : 1 ;
}

int sse_init(sse_epoll_t *p, uint32_t maxevents)
{
    if (!p || maxevents < 1)
        return (errno = EINVAL, 0) ;

    memset(p, 0, sizeof(*p)) ;
    p->maxevents = maxevents > SSE_MAX_EVENTS || !maxevents ? SSE_MAX_EVENTS : maxevents ;
    memset(p->ea, 0, sizeof(struct epoll_event) * p->maxevents) ;
    p->pending = queue_init(p->maxevents, &sse_watcher_cmp, &sse_watcher_rmcmp) ;
    if (p->pending == NULL)
        log_warnusys_return(LOG_EXIT_ZERO, "init pending queue") ;

    if (!hash_init(&p->watchers, 0, offsetof(struct sse_watcher_hash_s, node)))
        log_warnusys_return(LOG_EXIT_ZERO, "init watchers hash") ;

    p->file = queue_init(p->maxevents, &sse_watcher_cmp, &sse_watcher_rmcmp) ;
    if (p->file == NULL)
        log_warnusys_return(LOG_EXIT_ZERO, "init file queue") ;

    p->deferred = malloc(p->maxevents * sizeof(sse_deferred_t)) ;
    if (p->deferred == NULL)
        log_warnusys_return(LOG_EXIT_ZERO, "init deferred list") ;
    p->deferred_cap = p->maxevents ;
    p->ndeferred = 0 ;

    p->rerun_file = false ;
    p->running = false ;
    p->fd = -1 ;
    return 1 ;
}

int sse_new(sse_epoll_t *p, uint32_t maxevents)
{
    if (!sse_init(p, !maxevents ? SSE_MAX_EVENTS : maxevents))
        return 0 ;

    int e = errno ;
    errno = 0 ;
    p->fd = epoll_create1(EPOLL_CLOEXEC) ;
    if (p->fd < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "create epoll") ;

    errno = e ;

    p->running = true ;

    return 1 ;
}

int sse_wait(sse_epoll_t *p, int timeout)
{
    int nfds = -1, pos = 0 ;

    while ((nfds = epoll_wait(p->fd, p->ea, p->maxevents, timeout)) < 0) {

        if (!p->running)
            break ;

        if (errno == EINTR)
            continue ;

        log_warnusys_return(LOG_EXIT_LESSONE, "epoll_wait") ;
    }

    for (; pos < nfds ; pos++) {
        sse_watcher_t *w = p->ea[pos].data.ptr ;
        w->revents = p->ea[pos].events ;
        if (!queue_insert_ptr(p->pending, w))
            log_warnusys("insert watcher to pending list") ;
    }

    // ordering by priority
    if (p->pending->size) {
        if (!queue_sort(p->pending))
            log_warnusys("sort pending list") ;
    }

    return nfds ;
}

void sse_sanitize(sse_epoll_t *p)
{
    sse_watcher_t *w ;
    if (p->pending->size) {
        /** avoid starvation of events if any */
        FOREACH_QUEUE(p->pending, w)
            if (w->priority > 0)
                w->priority-- ;
    }

    sse_watcher_file(p) ;
}

int sse_prepare(sse_epoll_t *p)
{
    if (!hash_count(&p->watchers))
        return 1 ;

    int r = 1, e = 0 ;
    struct sse_watcher_hash_s *h, *tmp ;
    HASH_FOREACH(&p->watchers, h, tmp) {
        // zeroing epl and api error
        sse_err_zero((h->w)) ;
        switch (h->w->type) {

            case SSE_TYPE_SCHEDULE:

                if (sse_watcher_active(h->w)) {

                    uint64_t expirations;
                    ssize_t r = read(h->w->fd, &expirations, sizeof(expirations));
                    if (r < 0 && errno == ECANCELED) {

                        time_t when = sse_getfire_schedule(h->w) ;
                        if (!sse_modify_schedule(h->w, when)) {
                            r = 0 ;
                            e = errno ;
                            sse_stop_schedule(h->w) ;
                            sse_err(h->w, e) ;
                        }
                    }
                }

                break ;

            default:
                break ;
        }

    }

    return r ;
}

static void sse_dispatch_io(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("io watcher is NULL") ;
        return ;
    }

    if (w->revents & (SSE_ERROR)) {
        sse_stop_io(w) ;
        sse_err_zero(w) ;
    }
}

static void sse_dispatch_signal(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("signal watcher is NULL") ;
        return ;
    }

    if (!w->sdata) {
        sse_err(w, EINVAL);
        sse_stop_child(w);
        return;
    }

    if (w->revents & SSE_HUP) {
        sse_stop_signal(w) ;
        sse_err(w, ENODEV) ; // no signal delivery possible
        return ;
    }

    sse_signal_t *s = (sse_signal_t *)w->sdata ;
    struct signalfd_siginfo si ;
    ssize_t n ;

    while ((n = lx_signalfd_read(&si)) > 0)
        s->si = si ;  // Coalesce: keep last signal

    if (n < 0) {
        sse_err(w, errno) ;
        sse_stop_signal(w) ;
    }
}

static void sse_dispatch_child(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("child watcher is NULL") ;
        return ;
    }

    if (!w->sdata) {
        sse_err(w, EINVAL);
        sse_stop_child(w);
        return;
    }

    if (w->revents & (SSE_HUP | SSE_ERROR)) {
        // Reap zombie if possible
        pid_t pid = ((sse_child_t *)w->sdata)->pid ;
        int wstat ;
        waitpid(pid, &wstat, WNOHANG) ;
        sse_stop_child(w) ;
        sse_err_zero(w) ;
        return ;
    }

    pid_t pid = ((sse_child_t *)w->sdata)->pid ;
    int wstat ;

    int r = waitpid(pid, &wstat, WNOHANG) ;
    if (r > 0) {
        // Child exited, store status
        ((sse_child_t *)w->sdata)->status = wstat ;

    } else if (!r) {
        // Child still running
        errno = 0 ;
        sse_err_zero(w) ;

    } else if (errno != EAGAIN && errno != ECHILD) {
        if (errno == EBADF || errno == EINVAL) {
            sse_err(w, errno) ;
            sse_stop_child(w) ;
        }
    }
}

static void sse_dispatch_inotify(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("inotify watcher is NULL") ;
        return ;
    }

    if (w->revents & (SSE_HUP | SSE_ERROR)) {
        sse_stop_inotify(w) ;
        sse_err(w, EBADF) ; // inotify instance dead
        return ;
    }

    ssize_t rlen = lx_inotify_read() ;
    if (rlen < 0) {
        sse_err(w, errno) ;
        sse_stop_inotify(w) ;
    }
}

static void sse_dispatch_timer(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("timerfd watcher is NULL") ;
        return ;
    }

    if (w->revents & SSE_HUP) {
        sse_stop_timer(w) ;
        sse_err_zero(w) ;
        return ;
    }

    uint64_t expirations ;
    ssize_t r = read(w->fd, &expirations, sizeof(expirations)) ;
    if (r < 0) {
        if (errno == EBADF || errno == EINVAL) {
            sse_err(w, errno) ;
            sse_stop_timer(w) ;
        }
    }
}

static void sse_dispatch_schedule(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("schedule watcher is NULL") ;
        return ;
    }
    /* On a discontinuous CLOCK_REALTIME change a TFD_TIMER_CANCEL_ON_SET timer
     * is cancelled and read() fails with ECANCELED -- it never returns 0. */
    uint64_t expirations ;
    ssize_t r = read(w->fd, &expirations, sizeof(expirations)) ;
    if (r < 0) {
        int e = errno ;

        if (e == ECANCELED) {
            /** The clock was set: recompute the next fire time against the new
             * clock (cron_next, via sse_getfire_schedule) and rearm. Not an
             * error. */
            time_t when = sse_getfire_schedule(w) ;

            if (!sse_modify_schedule(w, when)) {
                sse_err(w, errno) ;
                sse_stop_schedule(w) ;
                return ;
            }

            ((sse_schedule_t *)w->sdata)->clockchange = true ;

            sse_err_zero(w) ;
            errno = 0 ;
            return ;
        }

        if (e == EBADF || e == EINVAL)
            sse_stop_schedule(w) ;

        sse_err(w, e) ;
        return ;
    }
}

static void sse_dispatch_eventfd(sse_watcher_t *w)
{
    if (!w) {
        errno = EINVAL ;
        log_warnsys("eventfd watcher is NULL") ;
        return ;
    }

    if (w->revents & (SSE_HUP | SSE_ERROR)) {
        sse_stop_eventfd(w) ;
        sse_err(w, EBADF) ;
        return ;
    }

    uint64_t counter;
    ssize_t r = read(w->fd, &counter, sizeof(counter)) ;
    if (r < 0) {
        int e = errno ;
        if (errno == EBADF || errno == EINVAL)
            sse_stop_eventfd(w) ;

        sse_err(w, e) ;
    }
}

int sse_defer(sse_epoll_t *p, void (*fn)(void *), void *arg)
{
    if (!p || !fn)
        return (errno = EINVAL, 0) ;

    if (p->ndeferred >= p->deferred_cap) {
        size_t cap = p->deferred_cap ? p->deferred_cap * 2 : NPLUS ;
        sse_deferred_t *d = realloc(p->deferred, cap * sizeof(sse_deferred_t)) ;
        if (!d)
            return (errno = ENOMEM, 0) ;
        p->deferred = d ;
        p->deferred_cap = cap ;
    }

    p->deferred[p->ndeferred].fn = fn ;
    p->deferred[p->ndeferred].arg = arg ;
    p->ndeferred++ ;

    return 1 ;
}

/* Run every deferred action then clear the list. Called once the dispatch (or
 * sse_free) no longer touches any watcher. The loop re-reads p->deferred each
 * step, so a deferred fn that itself calls sse_defer (and triggers a realloc)
 * stays safe; such re-deferred actions run within this same drain. */
static void sse_drain_deferred(sse_epoll_t *p)
{
    for (size_t i = 0 ; i < p->ndeferred ; i++)
        p->deferred[i].fn(p->deferred[i].arg) ;

    p->ndeferred = 0 ;
}

void sse_dispatch(sse_epoll_t *p)
{
    if (!p->pending->size)
        return ;

    sse_watcher_t *w ;
    sse_watcher_t *remove[p->pending->size + 1];
    size_t n = 0 ;

    FOREACH_QUEUE(p->pending, w) {

        if (p->running == false)
            break ;

        if (w->active == true) {

            switch (w->type) {

                case SSE_TYPE_IO:
                    sse_dispatch_io(w) ;
                    break ;

                case SSE_TYPE_SIGNAL:
                    sse_dispatch_signal(w) ;
                    break ;

                case SSE_TYPE_CHILD:
                    sse_dispatch_child(w) ;
                    break ;

                case SSE_TYPE_INOTIFY:
                    sse_dispatch_inotify(w) ;
                    break ;

                case SSE_TYPE_TIMER:
                    sse_dispatch_timer(w) ;
                    break ;

                case SSE_TYPE_SCHEDULE:
                    sse_dispatch_schedule(w) ;
                    break ;

                case SSE_TYPE_EVENTFD:
                    sse_dispatch_eventfd(w) ;
                    break ;

                default:
                    break ;
            }
        }
        /** Always call the callback even for an inactive event.
         * The callback is responsible to check error and freeing
         * the watchers if its needed.
         *
         * Everything that touches w must happen BEFORE the callback: a
         * callback may free the memory holding w (self-free), so the loop
         * must not read or write w once w->cb has returned. The post-loop
         * queue_remove_ptr only compares pointers, so it stays safe. */
        remove[n++] = w ;
        int rev = w->revents & SSE_MASK ;
        w->revents = 0 ;
        if (w->cb) {
            if (clock_gettime(CLOCK_MONOTONIC, &w->time.dispatched) < 0)
                log_warnusys("get time to set event dispatched time") ;

            w->cb(w, w->cbdata, rev) ;
        }
    }

    for (size_t i = 0; i < n ; i++) {
        if (!queue_remove_ptr(p->pending, remove[i]))
            log_warnusys("remove watcher pointer from pending queue") ;
    }

    /* Safe point: no watcher of the pending queue is touched anymore. Run the
     * frees/actions a callback deferred during this dispatch. */
    sse_drain_deferred(p) ;
}

int sse_run(sse_epoll_t *p, int timeout)
{
    int nfds = 0 ;

    if (!p || p->fd < 0)
        return (errno = EINVAL, 0) ;

    do {
        sse_sanitize(p) ;
    } while (p->rerun_file) ;

    if (!sse_prepare(p))
        log_warnusys("prepare watchers") ;

    nfds = sse_wait(p, timeout) ;
    if (nfds < 0)
        return 0 ;

    if (!nfds)
        return 1 ;

    sse_dispatch(p) ;

    return 1 ;
}

int sse_poll(sse_epoll_t *p, int timeout)
{
    if (!p || p->fd < 0)
        return (errno = EINVAL, 0) ;

    int e = 1 ;

    p->running = true ;

    while(p->running) {

        if (!sse_run(p, timeout)) {
            e = 0 ;
            break ;
        }
    }

    p->running = false ;

    return e ;
}

void sse_free(sse_epoll_t *p)
{
    if (!p)
       return ;

    /* Run any still-pending deferred action first, while the loop state is
     * coherent, so a deferred free (e.g. a stream closed just before shutdown)
     * is not leaked. */
    sse_drain_deferred(p) ;

    struct sse_watcher_hash_s *w, *tmp ;
    HASH_FOREACH(&p->watchers, w, tmp) {

        switch (w->w->type) {

            case SSE_TYPE_IO:
                sse_free_io(w->w) ;
                break ;

            case SSE_TYPE_SIGNAL:
                sse_free_signal(w->w) ;
                break ;

            case SSE_TYPE_CHILD:
                sse_free_child(w->w) ;
                break ;

            case SSE_TYPE_INOTIFY:
                sse_free_inotify(w->w);
                break ;

            case SSE_TYPE_TIMER:
                sse_free_timer(w->w) ;
                break ;

            case SSE_TYPE_SCHEDULE:
                sse_free_schedule(w->w) ;
                break ;

            case SSE_TYPE_EVENTFD:
                sse_free_eventfd(w->w) ;
                break ;

            default:
                break ;
        }
    }

    p->running = false ;
    queue_free_ptr(p->pending) ;
    p->pending = NULL ;
    sse_hash_free(&p->watchers);
    queue_free_ptr(p->file) ;
    p->file = NULL ;
    free(p->deferred) ;
    p->deferred = NULL ;
    p->ndeferred = 0 ;
    p->deferred_cap = 0 ;
    p->maxevents = 0 ;
    p->rerun_file = false ;
    if (p->fd >= 0)
        close(p->fd) ;

    p->fd = -1 ;
}
