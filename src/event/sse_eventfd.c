/*
 * sse_eventfd.c
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
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

#include <oblibs/log.h>

#include <oblibs/sse.h>

int sse_start_eventfd(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int priority)
{
	if (!w || !p) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "eventfd watcher is NULL") ;
    }

	struct sse_watcher_hash_s *h = sse_hash_search(p, w) ;
	if (h != NULL)
		return 1 ;

	int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC) ;
	if (fd < 0)
		log_warnusys_return(LOG_EXIT_ZERO, "eventfd") ;

	if (!sse_watcher_init(p, w, SSE_TYPE_EVENTFD, cb, cbdata, fd, SSE_READ, priority)) {
		close(fd) ;
		log_warnusys_return(LOG_EXIT_ZERO, "initiate eventfd watcher") ;
	}

	return sse_watcher_add(w) ;
}

int sse_restart_eventfd(sse_watcher_t *w)
{
	return sse_watcher_restart(w) ;
}

int sse_stop_eventfd(sse_watcher_t *w)
{
	return sse_watcher_stop(w) ;
}

int sse_free_eventfd(sse_watcher_t *w)
{
	return sse_watcher_free(w) ;
}

int sse_write_eventfd(sse_watcher_t *w)
{
	if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "eventfd watcher is NULL") ;
    }

	sse_err_zero(w) ;
	if (w->fd < 0) {
		sse_err_return(w, EINVAL) ;
	}

	uint64_t val = 1 ;

	if (write(w->fd, &val, sizeof(val)) != sizeof(val)) {
		log_warnusys("write to eventfd") ;
		int e = errno ;
		sse_stop_eventfd(w) ;
		sse_err_return(w, e) ;
	}

	return 1 ;
}

int sse_read_eventfd(sse_watcher_t *w)
{
	if (!w) {
        errno = EINVAL ;
        log_warnsys_return(LOG_EXIT_ZERO, "eventfd watcher is NULL") ;
    }

	sse_err_zero(w) ;
	if (w->fd < 0) {
		sse_err_return(w, EINVAL) ;
	}

	uint64_t val ;
	ssize_t n = read(w->fd, &val, sizeof(val));
    if (n < 0 || n != sizeof(val)) {
		log_warnusys("read from eventfd") ;
        int e = errno ;
		sse_stop_eventfd(w) ;
		sse_err_return(w, e) ;
    }

	return 1 ;
}