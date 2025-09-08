/*
 * linux.h
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

#ifndef OB_LINUX_H
#define OB_LINUX_H

#include <stdint.h>
#include <sys/inotify.h>
#include <oblibs/hash.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <limits.h> // NAME_MAX
#include <stdbool.h>

extern int lx_signalfd_init(void) ;
extern ssize_t lx_signalfd_read(struct signalfd_siginfo *si) ;
extern int lx_signalfd_add(int sig) ;
extern int lx_signalfd_del(int sig) ;
extern int lx_signalfd_ignore(int sig) ;
extern void lx_signalfd_end(void) ;
extern int lx_signalfd_get_fd(void) ;
extern sigset_t lx_signalfd_get_mask(void) ;

#ifndef NAME_MAX
#define NAME_MAX 256
#endif

#define INOTIFY_BUF_SIZE (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

struct lx_inotify_s
{
    int fd ;
    char buf[INOTIFY_BUF_SIZE] __attribute__((aligned(__alignof__(struct inotify_event)))) ;
    ssize_t blen ;
} ;

struct lx_inotify_wd_s
{
    int wd ;
    char *path ;
    UT_hash_handle hh_path ; // For path-based hash table
    UT_hash_handle hh_wd ; // For wd-based hash table
} ;
typedef struct lx_inotify_wd_s lx_inotify_wd_t ;

extern lx_inotify_wd_t *iwd ;        // Path-based hash table
extern lx_inotify_wd_t *iwd_by_wd ;  // wd-based hash table

#define FOREACH_INOTIFY() \
    for (struct inotify_event *event = NULL; lx_inotify_iterator(&event);)

extern int lx_inotify_init(void) ;
extern int lx_inotify_read(void) ;
extern int lx_inotify_add(const char *what, uint32_t imask) ;
extern int lx_inotify_update(const char *what, uint32_t imask) ;
extern int lx_inotify_del(const char *what) ;
extern void lx_inotify_end(void) ;
extern int lx_inotify_search(struct lx_inotify_wd_s **wd, const char *what) ;
extern int lx_inotify_get_fd(void) ;
extern char *lx_inotify_get_buffer(void) ;
extern int lx_inotify_get_blength(void) ;
extern lx_inotify_wd_t *lx_inotify_get_wd(int wd) ;
extern bool lx_inotify_iterator(struct inotify_event **event) ;

#endif