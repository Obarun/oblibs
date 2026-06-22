/*
 * lx_inotify.c
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>

#include <oblibs/hash.h>
#include <oblibs/linux.h>
#include <oblibs/io.h>

static struct lx_inotify_s ei = { .fd = -1 } ;
hash_t iwd = HASH_ZERO ; // Path-based hash table
hash_t iwd_by_wd = HASH_ZERO ;  // wd-based hash table

bool lx_inotify_iterator(struct inotify_event **event)
{
    static size_t len = 0 ;

    // Reset iterator when event is NULL (start of iteration)
    if (!*event) {
        len = 0 ;
    }

    if (len >= (size_t)ei.blen) {
        return false ;
    }

    for (; len < (size_t)ei.blen ; ) {
        // Check if there's enough space for the event structure
        if (len + sizeof(struct inotify_event) > (size_t)ei.blen) {
            len = ei.blen ;
            return false ;
        }

        *event = (struct inotify_event *)&ei.buf[len] ;

        // Validate event->len
        size_t name_len = (*event)->len ;
        if (name_len > (size_t)ei.blen - len - sizeof(struct inotify_event)) {
            len = ei.blen ;
            return false ;
        }

        // Advance len
        size_t event_size = sizeof(struct inotify_event) + name_len ;
        len += event_size ;

        // Skip events with no mask or IN_IGNORED
        if (!(*event)->mask || (*event)->mask & IN_IGNORED) {
            continue ;
        }

        return true ;
    }

    return false ;
}

int lx_inotify_get_fd(void)
{
    return ei.fd ;
}

char *lx_inotify_get_buffer(void)
{
    return ei.buf ;
}

int lx_inotify_get_blength(void)
{
    return ei.blen ;
}

lx_inotify_wd_t *lx_inotify_get_wd(int wd)
{
    struct lx_inotify_wd_s *w ;
    w = hash_find(&iwd_by_wd, &wd, sizeof(int)) ;
    return w ;
}

int lx_inotify_init(void)
{
    if (ei.fd >= 0)
        return ei.fd ;

    ei.fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK) ; // IN_NONBLOCK should be not necessary with epoll
    if (ei.fd < 0)
        return ei.fd ;

    // hash needs an explicit init (uthash created the tables lazily on first add).
    if (!hash_init(&iwd, 0, offsetof(struct lx_inotify_wd_s, node_path)) ||
        !hash_init(&iwd_by_wd, 0, offsetof(struct lx_inotify_wd_s, node_wd))) {
            hash_free(&iwd) ;
            hash_free(&iwd_by_wd) ;
            close(ei.fd) ;
            ei.fd = -1 ;
            return -1 ;
    }

    return ei.fd ;
}

int lx_inotify_read(void)
{
    ei.blen = io_read(ei.fd, ei.buf, sizeof(ei.buf) - 1) ;
    if (ei.blen < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ei.blen = 0 ;
            return 0 ; // No events available
        }
        return -1 ; // Fatal error, preserve errno
    }

    ei.buf[ei.blen] = 0 ;
    return (int)ei.blen ;
}

int lx_inotify_add(const char *what, uint32_t imask)
{
    if (ei.fd == -1)
        return (errno = EBADF, 0) ;

    struct lx_inotify_wd_s *wd ;
    wd = hash_find(&iwd, what, strlen(what)) ;
    if (wd != NULL)
        return wd->wd ;

    int w = inotify_add_watch(ei.fd, what, imask) ;
    if (w < 0)
        return 0 ;

    wd = malloc(sizeof(*wd)) ;
    if (!wd)
        return (errno = ENOMEM, 0) ;

    wd->wd = w ;
    wd->path = strdup(what) ;
    if (wd->path == NULL) {
        free(wd) ;
        return (errno = ENOMEM, 0) ;
    }

    hash_add(&iwd, wd->path, strlen(wd->path), wd) ; // Path-based hash table, node_path
    hash_add(&iwd_by_wd, &wd->wd, sizeof(int), wd) ; // wd-based hash table, node_wd

    return w ;
}

int lx_inotify_update(const char *what, uint32_t imask)
{
    if (ei.fd == -1)
        return (errno = EBADF, 0) ;

    struct lx_inotify_wd_s *wd ;
    wd = hash_find(&iwd, what, strlen(what)) ;
    if (wd == NULL)
        return (errno = ENOENT, 0) ;

    int w = inotify_add_watch(ei.fd, what, imask) ;
    if (w < 0)
        return 0 ;

    /** Updates wd->wd in case inotify_add_watch
     * returns a new descriptor (though this is rare with inotify)
     */
    if (w != wd->wd) {
        hash_del(&iwd_by_wd, wd) ;
        wd->wd = w ;
        hash_add(&iwd_by_wd, &wd->wd, sizeof(int), wd) ;
    }

    return w ;
}

int lx_inotify_del(const char *what)
{
    if (ei.fd < 0)
        return (errno = EBADF, 0) ;

    struct lx_inotify_wd_s *wd  ;
    if (!lx_inotify_search(&wd, what))
        return 1 ;

    if (inotify_rm_watch(ei.fd, wd->wd) < 0)
        return 0 ;

    hash_del(&iwd, wd) ;
    hash_del(&iwd_by_wd, wd) ;
    free(wd->path) ;
    free(wd) ;

    return 1 ;
}

int lx_inotify_search(struct lx_inotify_wd_s **wd, const char *what)
{
    if (ei.fd < 0)
        return (errno = EBADF, 0) ;

    *wd = hash_find(&iwd, what, strlen(what)) ;
    if ((*wd) == NULL)
        return (errno = ENOENT, 0) ;

    return 1 ;
}

void lx_inotify_end(void)
{
    struct lx_inotify_wd_s *c, *tmp ;
    HASH_FOREACH(&iwd, c, tmp) {
        free(c->path) ;
        free(c) ;
    }

    hash_free(&iwd) ; // free the bucket arrays (uthash freed at last DELETE)
    hash_free(&iwd_by_wd) ;

    if (ei.fd >= 0)
        close(ei.fd) ;

    ei.fd = -1 ;
    ei.blen = 0 ;
}
