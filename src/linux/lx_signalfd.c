/*
 * lx_signalfd.c
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
 *
 * The following functions are largely inspired by
 * https://skarnet.org/software/skalibs/libstddjb/selfpipe.html
 *
 * */

#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <oblibs/io.h>

struct lx_signalfd_s
{
  sigset_t mask ;
  int fd ;
} ;

static struct lx_signalfd_s sfd = { .fd = -1 } ;

static void lx_signalfd_nop(int sig)
{
    (void)sig;
}

int lx_signalfd_init(void)
{
    if (sfd.fd >= 0)
        return sfd.fd ;

    sigemptyset(&sfd.mask) ;
    sfd.fd = signalfd(-1, &sfd.mask, SFD_NONBLOCK | SFD_CLOEXEC) ;

    return sfd.fd ;
}

/**
 * @return 1 if a signal was read
 *         0 if no signal is pending (EAGAIN/EWOULDBLOCK)
 *        -1 on fatal error (e.g., EBADF)
 */
ssize_t lx_signalfd_read(struct signalfd_siginfo *si)
{
    ssize_t fr = io_read(sfd.fd, (char *)si, sizeof(*si)) ;
    ssize_t r = io_read_exit(fr) ;
    return (r <= 0) ? r : 1 ;
}

int lx_signalfd_add(int sig)
{
    if (sfd.fd == -1)
        return (errno = EBADF, 0) ;

    if (sig < 1 || sig >= NSIG)
        return (errno = EINVAL, 0) ;

    if (sigismember(&sfd.mask, sig))
        return 1 ;

    sigset_t cmask = sfd.mask, omask ;

    if ((sigaddset(&cmask, sig) < 0) ||
        (sigprocmask(SIG_BLOCK, &cmask, &omask) < 0))
        return 0 ;

    if (signalfd(sfd.fd, &cmask, SFD_NONBLOCK | SFD_CLOEXEC) < 0) {

        int e = errno ;
        sigprocmask(SIG_SETMASK, &omask, 0) ;
        errno = e ;
        return 0 ;
    }

    sfd.mask = cmask ;

    return 1 ;
}

int lx_signalfd_del(int sig)
{
    sigset_t cmask = sfd.mask ;
    sigset_t ublock ;

    if (sfd.fd == -1)
        return (errno = EBADF, 0) ;

    if (sig < 1 || sig >= NSIG)
        return (errno = EINVAL, 0) ;

    int r = sigismember(&sfd.mask, sig) ;
    if (r < 0)
        return 0 ;

    if (!r)
        return 1 ;

    if ((sigdelset(&cmask, sig) < 0) ||
        (signalfd(sfd.fd, &cmask, SFD_NONBLOCK | SFD_CLOEXEC) < 0))
        return 0 ;

    sigemptyset(&ublock) ;
    sigaddset(&ublock, sig) ;

    if (sigprocmask(SIG_UNBLOCK, &ublock, 0) < 0) {
        int e = errno ;
        signalfd(sfd.fd, &sfd.mask, SFD_NONBLOCK | SFD_CLOEXEC) ;
        errno = e ;
        return 0 ;
    }

    sfd.mask = cmask ;
    return 1 ;
}

int lx_signalfd_ignore(int sig)
{
    struct sigaction action = {
        .sa_handler = lx_signalfd_nop,
        .sa_flags = SA_RESTART | SA_NOCLDSTOP
    } ;
    sigfillset(&action.sa_mask) ;

    int ret = sigaction(sig, &action, NULL) ;
    if (ret >= 0)
        return 1 ;

    if (errno == EINVAL && sig >= 1 && sig < NSIG)
        return 1 ; // Non-catchable signal (e.g., SIGKILL, SIGSTOP)

    return 0 ;
}

void lx_signalfd_end(void)
{
    sigprocmask(SIG_UNBLOCK, &sfd.mask, 0) ;
    sigemptyset(&sfd.mask) ;
    if (sfd.fd >= 0)
        close(sfd.fd) ;
    sfd.fd = -1 ;
}

sigset_t lx_signalfd_get_mask(void)
{
    return sfd.mask ;
}

int lx_signalfd_get_fd(void)
{
    return sfd.fd ;
}