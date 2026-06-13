/*
 * io_rb.c
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

#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#include <oblibs/rbuffer.h>
#include <oblibs/io.h>
#include <oblibs/io_rb.h>

size_t io_rb_iovec_len(const struct iovec *iov, int iovcnt)
{
    size_t total = 0 ;
    for (int i = 0 ; i < iovcnt ; i++) {
        total += iov[i].iov_len ;
    }
    return total ;
}

int io_rb_init(io_rb_t *io, const io_rb_ops_t *ops, int fd)
{
    if (!io)
        return (errno = EINVAL, 0) ;

    memset(io, 0, sizeof(*io)) ;

    if (!rbuffer_init(&io->in.buf, io->in.data, sizeof(io->in.data)) ||
        !rbuffer_init(&io->out.buf, io->out.data, sizeof(io->out.data)) ||
        !rbuffer_init(&io->fdin.buf, io->fdin.data, sizeof(io->fdin.data)) ||
        !rbuffer_init(&io->fdout.buf, io->fdout.data, sizeof(io->fdout.data)))
        return 0 ;

    io->ops = ops ? ops : NULL ;
    io->fd = fd ;
    io->want_flush = false ;
    io->refcount = 1 ;

    return 1 ;
}

ssize_t io_rb_read(io_rb_t *io, void *data)
{
    if (!io || io->fd < 0)
        return (errno = EBADF, -1) ;

    ssize_t len ;

    if (rbuffer_isfull(&io->in.buf))
        return (errno = EOVERFLOW, -1) ;

    // Get writable regions from ring buffer
    struct iovec iov[2] ;
    unsigned iovcnt = rbuffer_wiovec(&io->in.buf, iov) ;

    if (!iovcnt)
        return (errno = EOVERFLOW, -1) ;

    if (io->ops && io->ops->read) {

        len = io->ops->read(io, iov, iovcnt, data) ;

        if (len < 0) {
            if (error_isagain(errno))
                return 0 ;
            return -1 ;
        }

    } else {

        char cmsg[CMSG_SPACE(IO_RB_MAX_FD * sizeof(int))] ;
        struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = iov,
            .msg_iovlen = iovcnt,
            .msg_control = cmsg,
            .msg_controllen = sizeof(cmsg),
            .msg_flags = 0,
        } ;

        do {
            len = recvmsg(io->fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC) ;
        } while (len < 0 && errno == EINTR) ;

        if (len < 0) {
            if (error_isagain(errno))
                return 0 ;
            return -1 ;
        }

        // Process received file descriptors
        if (io_rb_decode_cmsg(io, &msg) != 0)
            return -1 ;
    }

    // Advance write pointer
    rbuffer_wadvance(&io->in.buf, len) ;

    return len ;

}

ssize_t io_rb_flush(io_rb_t *io, void *data)
{
    if (!io)
        return (errno = EBADF, -1) ;

    if (!io->want_flush)
        return 0 ;

    bool dfault = (io->ops && io->ops->write) ? false : true ;

    if (rbuffer_isempty(&io->out.buf)) {
        /** No payload to carry the queued fds. On a stream socket SCM_RIGHTS
         * requires at least one data byte as carrier, so fds sitting in fdout
         * with no data can never be transmitted. Surface the contract violation
         * instead of looping forever on a writable fd (busy-loop). */
        if (dfault && !rbuffer_isempty(&io->fdout.buf))
            return (errno = EINVAL, -1) ;
        return 0 ;
    }

    ssize_t total_sent = 0 ;

    while (!rbuffer_isempty(&io->out.buf)) {

        int nfd = 0, cmsglen ;
        ssize_t len, wr ;
        struct iovec iov[2] ;
        char cmsg[CMSG_SPACE(IO_RB_MAX_FD * sizeof(int))] ;
        unsigned iov_count = rbuffer_riovec(&io->out.buf, iov) ;

        if (!iov_count)
            break ;

        if (!dfault) {
            len = io->ops->write(io, iov, iov_count, data) ;
        } else {

            nfd = io_rb_encode_cmsg(io, cmsg, sizeof(cmsg), &cmsglen) ;
            if (nfd < 0)
                return (errno = EINVAL, -1) ;

            struct msghdr msg = {
                .msg_name = NULL,
                .msg_namelen = 0,
                .msg_iov = iov,
                .msg_iovlen = iov_count,
                .msg_control = (cmsglen > 0) ? cmsg : NULL,
                .msg_controllen = cmsglen,
                .msg_flags = 0,
            } ;

            do {
                len = sendmsg(io->fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT) ;
            } while (len < 0 && errno == EINTR) ;

        }

        wr = io_write_result(len) ;
        if (wr < 0)
            return -1 ;

        if (!wr)
            break ;

        rbuffer_radvance(&io->out.buf, len) ;
        if (dfault)
            rbuffer_radvance(&io->fdout.buf, nfd * sizeof(int)) ;

        total_sent += len ;
    }

    /** Only clear want_flush if data buffer is empty
     * Transports with ancillary data must clear it themselves*/
    if (rbuffer_isempty(&io->out.buf)) {
        io->want_flush = false ;
        /** Data fully drained but fds remain: they have lost their carrier and
         * can never be sent. The enqueue guard (io_rb_put_fd/io_rb_putv_fd)
         * caps pending fds to IO_RB_MAX_FD so a single iteration drains them,
         * making this unreachable in practice; kept as defense in depth. */
        if (dfault && !rbuffer_isempty(&io->fdout.buf))
            return (errno = EINVAL, -1) ;
    }

    return total_sent ;
}

int io_rb_put(io_rb_t *io, const void *data, size_t count, void *ctx)
{
    if (!io || !data)
        return (errno = EINVAL, -1) ;

    // Try to flush first if buffer is getting full
    if (rbuffer_available(&io->out.buf) < count) {
        io->want_flush = true ;
        if (io_rb_flush(io, ctx) < 0)
            return -1 ;

        if (rbuffer_available(&io->out.buf) < count)
            return (errno = EAGAIN, -1) ;
    }

    size_t written = rbuffer_write(&io->out.buf, data, count) ;
    if (written < count)
        return (errno = EOVERFLOW, -1) ;

    io->want_flush = true ;

    return 0 ;
}

int io_rb_putv(io_rb_t *io, const struct iovec *iov, int iovcnt, void *ctx)
{
    if (!io || !iov || iovcnt <= 0)
        return (errno = EINVAL, -1) ;

    size_t total_size = io_rb_iovec_len(iov, iovcnt) ;

    if (!total_size)
        return 0 ;

    if (total_size > rbuffer_capacity(&io->out.buf))
        return (errno = EOVERFLOW, -1) ;

    // Try to flush first if needed
    if (rbuffer_available(&io->out.buf) < total_size) {
        io->want_flush = true ;
        if (io_rb_flush(io, ctx) < 0)
            return -1 ;

        if (rbuffer_available(&io->out.buf) < total_size)
            return (errno = EAGAIN, -1) ;
    }

    size_t written = rbuffer_writev(&io->out.buf, iov, iovcnt) ;
    if (written < total_size)
        return (errno = EOVERFLOW, -1) ;

    io->want_flush = true ;

    return 0 ;
}

int io_rb_put_fd(io_rb_t *io, int fd)
{
    if (!io)
        return (errno = EINVAL, -1) ;

    /** A single sendmsg carries at most IO_RB_MAX_FD fds; the flush loop
     * drains fdout in one pass alongside the data. Cap pending fds here so
     * they can never outlive their data carrier (see io_rb_flush). */
    if ((io_rb_pending_fdout(io) / sizeof(int)) + 1 > IO_RB_MAX_FD)
        return (errno = EINVAL, -1) ;

    if (rbuffer_available(&io->fdout.buf) < sizeof(int))
        return (errno = EOVERFLOW, -1) ;

    size_t written = rbuffer_write(&io->fdout.buf, &fd, sizeof(int)) ;

    if (written < sizeof(int))
        return (errno = EOVERFLOW, -1) ;

    io->want_flush = true ;

    return 0 ;
}

int io_rb_putv_fd(io_rb_t *io, const struct iovec *iov, int iovcnt, const int *fds, int nfd, void *ctx)
{
    if (!io || !iov || iovcnt <= 0 || nfd < 0 || (nfd > 0 && !fds))
        return (errno = EINVAL, -1) ;

    /** A single sendmsg carries at most IO_RB_MAX_FD fds; the flush loop drains
     * fdout in one pass alongside the data. Reject batches that would push the
     * pending fd count past that cap so fds can never outlive their carrier. */
    if (nfd > IO_RB_MAX_FD || (io_rb_pending_fdout(io) / sizeof(int)) + nfd > IO_RB_MAX_FD)
        return (errno = EINVAL, -1) ;

    size_t total_data_size = io_rb_iovec_len(iov, iovcnt) ;
    size_t total_fd_size = nfd * sizeof(int) ;

    if (total_data_size > rbuffer_capacity(&io->out.buf))
        return (errno = EOVERFLOW, -1) ;

    if (total_fd_size > rbuffer_capacity(&io->fdout.buf))
        return (errno = EOVERFLOW, -1) ;

    bool need_flush = false ;
    if ((total_data_size > 0 && rbuffer_available(&io->out.buf) < total_data_size) ||
        (total_fd_size > 0 && rbuffer_available(&io->fdout.buf) < total_fd_size))
        need_flush = true ;

    if (need_flush) {
        io->want_flush = true ;
        if (io_rb_flush(io, ctx) < 0)
            return -1 ;

        // Verify we have sufficient space after flush
        if ((total_data_size > 0 && rbuffer_available(&io->out.buf) < total_data_size) ||
            (total_fd_size > 0 && rbuffer_available(&io->fdout.buf) < total_fd_size))
            return (errno = EAGAIN, -1) ;
    }

    if (total_data_size > 0) {
        size_t written = rbuffer_writev(&io->out.buf, iov, iovcnt) ;
        if (written < total_data_size)
            return (errno = EOVERFLOW, -1) ; // This should not happen
    }

    if (nfd > 0) {

        size_t fd_written = rbuffer_write(&io->fdout.buf, fds, total_fd_size) ;
        if (fd_written < total_fd_size) {
            /** This should not happen, but if it does, we need to rollback
             * the data write to maintain atomicity */
            if (total_data_size > 0)
                rbuffer_wrewind(&io->out.buf, total_data_size) ;

            return (errno = EOVERFLOW, -1) ;
        }
    }

    if (total_data_size > 0 || nfd > 0)
        io->want_flush = true ;

    return 0 ;
}

ssize_t io_rb_get(io_rb_t *io, void *dst, size_t count)
{
    if (!io || !dst)
        return (errno = EINVAL, -1) ;

    if (rbuffer_used(&io->in.buf) < count)
        return (errno = EAGAIN, -1) ;

    size_t read_bytes = rbuffer_read(&io->in.buf, dst, count) ;
    return read_bytes == count ? (ssize_t)read_bytes : -1 ;
}

ssize_t io_rb_get_fd(io_rb_t *io, int *fd)
{
    if (!io || !fd)
        return (errno = EINVAL, -1) ;

    if (rbuffer_used(&io->fdin.buf) < sizeof(int))
        return (errno = EAGAIN, -1) ;

    size_t read_bytes = rbuffer_read(&io->fdin.buf, fd, sizeof(int)) ;
    return read_bytes == sizeof(int) ? (ssize_t)read_bytes : -1 ;
}

size_t io_rb_pending_in(const io_rb_t *io)
{
    return io ? rbuffer_used(&io->in.buf) : 0 ;
}

size_t io_rb_pending_out(const io_rb_t *io)
{
    return io ? rbuffer_used(&io->out.buf) : 0 ;
}

size_t io_rb_pending_fdin(const io_rb_t *io)
{
    return io ? rbuffer_used(&io->fdin.buf) : 0 ;
}

size_t io_rb_pending_fdout(const io_rb_t *io)
{
    return io ? rbuffer_used(&io->fdout.buf) : 0 ;
}

void io_rb_restore(io_rb_t *io, size_t count)
{
    if (io)
        rbuffer_rrewind_safe(&io->in.buf, count) ;
}

ssize_t io_rb_reserve_output(io_rb_t *io, size_t count)
{
    if (!io)
        return (errno = EINVAL, -1) ;

    size_t reserved = rbuffer_wreserve(&io->out.buf, count) ;

    if (reserved > 0)
        io->want_flush = true ;

    return (ssize_t)reserved ;
}

void io_rb_ref(io_rb_t *io)
{
    if (!io) return ;
    io->refcount++ ;
}

void io_rb_unref(io_rb_t *io)
{
    if (!io) return ;
    io->refcount-- ;
}

int io_rb_encode_cmsg(io_rb_t *io, char *cmsg, size_t cmsgsize, int *cmsglen)
{
    *cmsglen = 0 ;
    size_t len, fds_available, maxfd, required_size, bytes_read ;
    fds_available = rbuffer_used(&io->fdout.buf) ;
    if (!fds_available)
        return 0 ;

    if (cmsgsize < CMSG_SPACE(0))
        return -1 ;  // Buffer too small even for header

    len = cmsgsize - CMSG_SPACE(0) + CMSG_LEN(0) ;
    len = (len - CMSG_LEN(0)) / sizeof(int) ;
    maxfd = (len < IO_RB_MAX_FD) ? len : IO_RB_MAX_FD ;

    if (!maxfd)
        return -1 ;  // No space for any FDs

    // Calculate actual FDs to send
    len = fds_available / sizeof(int) ;
    len = (len < maxfd) ? len : maxfd ;

    if (!len)
        return 0 ;

    required_size = CMSG_SPACE(len * sizeof(int)) ;
    if (required_size > cmsgsize)
        return -1 ;  // Should not happen, but be safe

    struct cmsghdr *cm = (struct cmsghdr *)cmsg ;
    cm->cmsg_level = SOL_SOCKET ;
    cm->cmsg_type = SCM_RIGHTS ;
    cm->cmsg_len = CMSG_LEN(len * sizeof(int)) ;

    /** Peek only: the file descriptors stay in fdout. io_rb_flush consumes
     * them via rbuffer_radvance ONLY after a successful sendmsg, so a failed
     * or EAGAIN send does not lose them. */
    bytes_read = rbuffer_peek(&io->fdout.buf, CMSG_DATA(cm), len * sizeof(int)) ;
    if (bytes_read != len * sizeof(int)) {
        // Ring buffer state changed between checks - should not happen
        *cmsglen = 0 ;
        return -1 ;
    }

    *cmsglen = cm->cmsg_len ;

    return len ;
}

int io_rb_decode_cmsg(io_rb_t *io, struct msghdr *msg)
{
    bool overflow = false ;
    struct cmsghdr *cmsg ;
    size_t fdbytes, available_space ;

    for (cmsg = CMSG_FIRSTHDR(msg) ; cmsg != NULL ; cmsg = CMSG_NXTHDR(msg, cmsg)) {

        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
            continue ;

        if (cmsg->cmsg_len < CMSG_LEN(0)) {
            overflow = true ;
            continue ;
        }

        fdbytes = cmsg->cmsg_len - CMSG_LEN(0) ;

        // Validate fd_bytes is a multiple of sizeof(int)
        if (fdbytes % sizeof(int) != 0) {
            // Corrupted message - close any partial FDs we can identify
            int *fds = (int *)CMSG_DATA(cmsg) ;
            size_t n = fdbytes / sizeof(int), pos = 0 ;
            for (; pos < n ; pos++) {
                close(fds[pos]) ;
            }
            continue ;
        }

        if ((fdbytes / sizeof(int)) == 0)
            continue ;

        available_space = rbuffer_available(&io->fdin.buf) ;

        if (fdbytes > available_space || overflow) {
            overflow = true ;
            int *fds = (int *)CMSG_DATA(cmsg) ;
            size_t n = fdbytes / sizeof(int), pos = 0 ;
            for (; pos < n ; pos++) {
                close(fds[pos]) ;
            }
        } else {
            rbuffer_write(&io->fdin.buf, CMSG_DATA(cmsg), fdbytes) ;
        }
    }

    return overflow ? (errno = EOVERFLOW, -1) : 0 ;
}

void io_rb_close_fds(rbuffer_t *buf)
{
    int fd ;
    while (rbuffer_used(buf) >= sizeof(int)) {
        if (rbuffer_read(buf, &fd, sizeof(int)) == sizeof(int)) {
            if (fd >= 0) {
                /** Valgrind will complain if the fd was
                 * already closed. If we try to check the validity
                 * of the fd by using if (fcntl(fd, F_GETFD) == -1),
                 * it will be worse because valgring will also
                 * complain about this syscall, so twice errors.
                 * But we don't care at all, we are safe here because
                 * close() is idempotent in practice calling it twice is harmless.
                 */
                close(fd) ;
                fd = -1 ;
            }
        }
    }
}
