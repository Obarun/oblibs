/*
 * sse_stream.c
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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <oblibs/log.h>
#include <oblibs/rbuffer.h>
#include <oblibs/socket.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>

// Forward declaration of internal callback
static void sse_stream_io_callback(sse_watcher_t *w, void *data, int revents) ;
static void sse_stream_cleanup_internal(sse_stream_t *stream) ;
static sse_stream_t *sse_stream_ref(sse_stream_t *stream) ;
static void sse_stream_unref(sse_stream_t *stream) ;

static void close_fd_from_buffer(rbuffer_t *buf)
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

int sse_streamux_create_server(const char *path, int backlog)
{
    if (!path)
        return (errno = EINVAL, -1) ;

    int fd = socketunix_create(O_CLOEXEC | O_NONBLOCK) ;
    if (fd < 0)
        return -1 ;

    if (socketunix_bind(fd, path) < 0) {
        close(fd) ;
        return -1 ;
    }

    if (socketunix_listen(fd, backlog ? backlog : SOCKETUNIX_BACKLOG) < 0) {
        close(fd) ;
        return -1 ;
    }

    return fd ;
}

int sse_streamux_create_client(const char *path)
{
    if (!path)
        return (errno = EINVAL, -1) ;

    int fd = socketunix_create(O_CLOEXEC | O_NONBLOCK) ;
    if (fd < 0)
        return (errno = EINVAL, -1) ;

    if (socketunix_connect(fd, path) < 0) {
        close(fd) ;
        return -1 ;
    }

    return fd ;
}

int sse_streamux_accept(int fd)
{
    if (fd < 0)
        return (errno = EINVAL, -1) ;

    return socketunix_accept(fd, O_CLOEXEC | O_NONBLOCK) ;
}

static int build_cmsg(rbuffer_t *fds_buf, char *cmsg_data, size_t cmsg_size, int *cmsg_len)
{
    *cmsg_len = 0 ;
    size_t len, fds_available, max_fds, required_size, bytes_read ;
    fds_available = rbuffer_used(fds_buf) ;
    if (fds_available == 0)
        return 0 ;  // No FDs to send

    // Calculate maximum FDs that can fit in the provided buffer
    if (cmsg_size < CMSG_SPACE(0))
        return -1 ;  // Buffer too small even for header

    len = cmsg_size - CMSG_SPACE(0) + CMSG_LEN(0) ;
    len = len - CMSG_LEN(0) ;
    len = len / sizeof(int) ;

    // Also respect system limits
    max_fds = (len < MAX_FDS) ? len : MAX_FDS ;

    if (!max_fds)
        return -1 ;  // No space for any FDs

    // Calculate actual FDs to send
    len = fds_available / sizeof(int) ;
    len = (len < max_fds) ? len : max_fds ;

    if (!len)
        return 0 ;

    // Validate the final message will fit
    required_size = CMSG_SPACE(len * sizeof(int)) ;
    if (required_size > cmsg_size)
        return -1 ;  // Should not happen, but be safe

    // Now it's safe to build the message
    struct cmsghdr *cmsg = (struct cmsghdr *)cmsg_data ;
    cmsg->cmsg_level = SOL_SOCKET ;
    cmsg->cmsg_type = SCM_RIGHTS ;
    cmsg->cmsg_len = CMSG_LEN(len * sizeof(int)) ;

    // Read FDs from ring buffer into cmsg - now guaranteed to fit
    bytes_read = rbuffer_read(fds_buf, CMSG_DATA(cmsg), len * sizeof(int)) ;
    if (bytes_read != len * sizeof(int)) {
        // Ring buffer state changed between checks - should not happen
        *cmsg_len = 0 ;
        return -1 ;
    }

    *cmsg_len = cmsg->cmsg_len ;

    return len ;
}

static int decode_cmsg(rbuffer_t *fds_buf, struct msghdr *msg)
{
    bool overflow = false ;
    struct cmsghdr *cmsg ;
    size_t fdbytes, available_space ;

    for (cmsg = CMSG_FIRSTHDR(msg) ; cmsg != NULL ; cmsg = CMSG_NXTHDR(msg, cmsg)) {

        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
            continue ;
        }

        if (cmsg->cmsg_len < CMSG_LEN(0)) {
            overflow = true ;
            continue ; // Invalid control message length
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

        available_space = rbuffer_available(fds_buf) ;

        if (fdbytes > available_space || overflow) {
            overflow = true ;
            // Close FDs we can't store
            int *fds = (int *)CMSG_DATA(cmsg) ;
            size_t n = fdbytes / sizeof(int), pos = 0 ;
            for (; pos < n ; pos++) {
                close(fds[pos]) ;
            }
        } else {
            // Store FDs in ring buffer
            rbuffer_write(fds_buf, CMSG_DATA(cmsg), fdbytes) ;
        }
    }

    if (overflow)
        return (errno = EOVERFLOW, -1) ;

    return 0 ;
}

int sse_stream_init(sse_stream_t *stream, int fd, sse_stream_cb_t *read_cb, sse_stream_cb_t *write_cb, sse_stream_cb_t *close_cb, sse_stream_error_cb_t *error_cb, void *data)
{
    if (!stream || fd < 0)
        return (errno = EINVAL, 0) ;

    memset(stream, 0, sizeof(*stream)) ;

    // Initialize ring buffers
    if (!rbuffer_init(&stream->in.buf, stream->in.data, sizeof(stream->in.data)) ||
        !rbuffer_init(&stream->out.buf, stream->out.data, sizeof(stream->out.data)) ||
        !rbuffer_init(&stream->fdin.buf, stream->fdin.data, sizeof(stream->fdin.data)) ||
        !rbuffer_init(&stream->fdout.buf, stream->fdout.data, sizeof(stream->fdout.data)))
            return 0 ;

    stream->fd = fd ;
    stream->want_flush = false ;
    stream->closed = false ;
    stream->error = 0 ;
    stream->refcount = 1 ;

    stream->read_cb = read_cb ;
    stream->write_cb = write_cb ;
    stream->close_cb = close_cb ;
    stream->error_cb = error_cb ;
    stream->data = data ;

    return 1 ;
}

int sse_stream_attach(sse_stream_t *stream, sse_epoll_t *p, int priority)
{
    return sse_start_io(p, &stream->watcher, sse_stream_io_callback, stream, stream->fd, SSE_READ | SSE_WRITE, priority) ;
}

int sse_stream_detach(sse_stream_t *stream)
{
    return sse_stop_io(&stream->watcher) ;
}

ssize_t sse_stream_read(sse_stream_t *stream)
{
    if (!stream || stream->closed)
        return (errno = EBADF, -1) ;

    if (rbuffer_isfull(&stream->in.buf))
        return (errno = EOVERFLOW, -1) ;

    // Get writable regions from ring buffer
    struct iovec iov[2] ;
    unsigned iov_count = rbuffer_wiovec(&stream->in.buf, iov) ;
    ssize_t len ;

    if (iov_count == 0)
        return (errno = EOVERFLOW, -1) ;

    // Prepare to receive file descriptors
    char cmsg[CMSG_SPACE(MAX_FDS * sizeof(int))] ;
    struct msghdr msg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = iov,
        .msg_iovlen = iov_count,
        .msg_control = cmsg,
        .msg_controllen = sizeof(cmsg),
        .msg_flags = 0,
    } ;

    do {
        len = recvmsg(stream->fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC) ;
    } while (len < 0 && errno == EINTR) ;

    if (len < 0) {
        if (error_isagain(errno))
            return 0 ;
        return -1 ;
    } ;

    // Process received file descriptors
    if (decode_cmsg(&stream->fdin.buf, &msg) != 0)
        return -1 ;

    // Advance write pointer in input buffer
    rbuffer_wadvance(&stream->in.buf, len) ;

    errno = 0 ;

    return len ;
}

ssize_t sse_stream_flush(sse_stream_t *stream)
{
    if (!stream || stream->closed)
        return (errno = EBADF, -1) ;

    if (!stream->want_flush || rbuffer_isempty(&stream->out.buf))
        return 0 ;

    ssize_t total_sent = 0 ;

    while (!rbuffer_isempty(&stream->out.buf)) {

        int nfd ;
        ssize_t len, wr ;
        struct iovec iov[2] ;
        int cmsg_len ;
        char cmsg[CMSG_SPACE(MAX_FDS * sizeof(int))] ;
        unsigned iov_count = rbuffer_riovec(&stream->out.buf, iov) ;

        if (iov_count == 0)
            break ;

        // Prepare control message for file descriptors
        nfd = build_cmsg(&stream->fdout.buf, cmsg, sizeof(cmsg), &cmsg_len) ;
        if (nfd < 0)
            return (errno = EINVAL, -1) ;

        struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = iov,
            .msg_iovlen = iov_count,
            .msg_control = (cmsg_len > 0) ? cmsg : NULL,
            .msg_controllen = cmsg_len,
            .msg_flags = 0,
        } ;

        do {
            len = sendmsg(stream->fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT) ;
        } while (len < 0 && errno == EINTR) ;

        wr = io_write_result(len) ;
        if (wr < 0)
            return -1 ;

        if (!wr)
            break ;

        // Advance read pointer in output buffer
        rbuffer_radvance(&stream->out.buf, len) ;
        if (nfd)
            rbuffer_radvance(&stream->fdout.buf, nfd * sizeof(int)) ;
        total_sent += len ;
    }

    if (rbuffer_isempty(&stream->out.buf))
        stream->want_flush = false ;

    return total_sent ;
}

int sse_stream_put(sse_stream_t *stream, const void *data, size_t count)
{
    if (!stream || !data || stream->closed)
        return (errno = EINVAL, -1) ;

    // Check if we need to flush first
    if (rbuffer_available(&stream->out.buf) < count) {
        stream->want_flush = true ;
        if (sse_stream_flush(stream) < 0)
            return -1 ;

        // be sure to have sufficient space
        if (rbuffer_available(&stream->out.buf) < count)
            return (errno = EAGAIN, -1) ;
    }

    // Write data to output buffer
    size_t written = rbuffer_write(&stream->out.buf, data, count) ;
    if (written < count)
        return (errno = EOVERFLOW, -1) ;

    stream->want_flush = true ;

    return 0 ;
}

int sse_stream_putv(sse_stream_t *stream, const struct iovec *iov, int iovcnt)
{
    if (!stream || !iov || iovcnt <= 0 || stream->closed)
        return (errno = EINVAL, -1) ;

    size_t total_size = sse_stream_iovec_len(iov, iovcnt) ;
    if (total_size == 0)
        return 0 ; // Nothing to write

    if (total_size > rbuffer_capacity(&stream->out.buf))
        return (errno = EOVERFLOW, -1) ;

    if (rbuffer_available(&stream->out.buf) < total_size) {
        stream->want_flush = true ;
        if (sse_stream_flush(stream) < 0)
            return -1 ;

        // Verify we have sufficient space after flush
        if (rbuffer_available(&stream->out.buf) < total_size)
            return (errno = EAGAIN, -1) ;
    }

    // Write all iovec entries atomically
    size_t written = rbuffer_writev(&stream->out.buf, iov, iovcnt) ;

    if (written < total_size)
        return (errno = EOVERFLOW, -1) ; // This should not happen but be defensive

    stream->want_flush = true ;

    return 0 ;
}

ssize_t sse_stream_get(sse_stream_t *stream, void *dst, size_t count)
{
    if (!stream || !dst || stream->closed)
        return (errno = EINVAL, -1) ;

    if (rbuffer_used(&stream->in.buf) < count)
        return (errno = EAGAIN, -1) ;

    size_t read_bytes = rbuffer_read(&stream->in.buf, dst, count) ;
    return read_bytes == count ? (ssize_t)read_bytes : -1 ;
}

int sse_stream_put_fd(sse_stream_t *stream, int fd)
{
    if (!stream || fd < 0 || stream->closed)
        return (errno = EINVAL, -1) ;

    if (rbuffer_available(&stream->fdout.buf) < sizeof(int))
        return (errno = EOVERFLOW, -1) ;

    size_t written = rbuffer_write(&stream->fdout.buf, &fd, sizeof(int)) ;

    if (written < sizeof(int))
        return (errno = EOVERFLOW, -1) ;

    stream->want_flush = true ;

    return 0 ;
}

int sse_stream_putv_fd(sse_stream_t *stream, const struct iovec *iov, int iovcnt, const int *fds, int fdcnt)
{
    if (!stream || !iov || iovcnt <= 0 || fdcnt < 0 || stream->closed)
        return (errno = EINVAL, -1) ;

    if (fdcnt > 0 && !fds)
        return (errno = EINVAL, -1) ;

    size_t total_data_size = sse_stream_iovec_len(iov, iovcnt) ;
    size_t total_fd_size = fdcnt * sizeof(int) ;

    if (total_data_size > rbuffer_capacity(&stream->out.buf))
        return (errno = EOVERFLOW, -1) ;

    if (total_fd_size > rbuffer_capacity(&stream->fdout.buf))
        return (errno = EOVERFLOW, -1) ;

    bool need_flush = false ;
    if (total_data_size > 0 && rbuffer_available(&stream->out.buf) < total_data_size)
        need_flush = true ;
    if (total_fd_size > 0 && rbuffer_available(&stream->fdout.buf) < total_fd_size)
        need_flush = true ;

    if (need_flush) {

        stream->want_flush = true ;
        if (sse_stream_flush(stream) < 0)
            return -1   ;

        // Verify we have sufficient space after flush
        if (total_data_size > 0 && rbuffer_available(&stream->out.buf) < total_data_size)
            return (errno = EAGAIN, -1) ;
        if (total_fd_size > 0 && rbuffer_available(&stream->fdout.buf) < total_fd_size)
            return (errno = EAGAIN, -1) ;
    }

    if (total_data_size > 0) {
        size_t written = rbuffer_writev(&stream->out.buf, iov, iovcnt) ;
        if (written < total_data_size)
            return (errno = EOVERFLOW, -1) ; // This should not happen
    }

    if (fdcnt > 0) {

        size_t fd_written = rbuffer_write(&stream->fdout.buf, fds, total_fd_size) ;
        if (fd_written < total_fd_size) {
            /** This should not happen, but if it does, we need to rollback
             * the data write to maintain atomicity */
            if (total_data_size > 0)
                rbuffer_wrewind(&stream->out.buf, total_data_size) ;

            return (errno = EOVERFLOW, -1) ;
        }
    }

    if (total_data_size > 0 || fdcnt > 0)
        stream->want_flush = true ;

    return 0 ;
}

ssize_t sse_stream_get_fd(sse_stream_t *stream, int *fd)
{
    if (!stream || !fd || stream->closed)
        return (errno = EINVAL, -1) ;

    if (rbuffer_used(&stream->fdin.buf) < sizeof(int))
        return (errno = EAGAIN, -1) ;

    size_t read_bytes = rbuffer_read(&stream->fdin.buf, fd, sizeof(int)) ;
    return read_bytes == sizeof(int) ? (ssize_t)read_bytes : -1 ;
}

void sse_stream_close_fds(sse_stream_t *stream)
{
    if (!stream)
        return ;

    close_fd_from_buffer(&stream->fdin.buf) ;
    close_fd_from_buffer(&stream->fdout.buf) ;
}

size_t sse_stream_pending_in(const sse_stream_t *stream)
{
    return stream ? rbuffer_used(&stream->in.buf) : 0 ;
}

size_t sse_stream_pending_out(const sse_stream_t *stream)
{
    return stream ? rbuffer_used(&stream->out.buf) : 0 ;
}

size_t sse_stream_pending_fdin(const sse_stream_t *stream)
{
    return stream ? rbuffer_used(&stream->fdin.buf) / sizeof(int) : 0 ;
}

size_t sse_stream_pending_fdout(const sse_stream_t *stream)
{
    return stream ? rbuffer_used(&stream->fdout.buf) / sizeof(int) : 0 ;
}

bool sse_stream_is_closed(const sse_stream_t *stream)
{
    return stream ? stream->closed : true ;
}

int sse_stream_get_error(const sse_stream_t *stream)
{
    return stream ? stream->error : EINVAL ;
}

size_t sse_stream_iovec_len(const struct iovec *iov, int iovcnt)
{
    size_t total = 0 ;
    for (int i = 0 ; i < iovcnt ; i++) {
        total += iov[i].iov_len ;
    }
    return total ;
}

void sse_stream_restore(sse_stream_t *stream, size_t count)
{
    if (!stream)
        return ;
    rbuffer_rrewind_safe(&stream->in.buf, count) ;
}

int sse_stream_reserve_output(sse_stream_t *stream, size_t count)
{
    if (!stream || stream->closed)
        return (errno = EINVAL, -1) ;

    size_t reserved = rbuffer_wreserve(&stream->out.buf, count) ;

    if (reserved > 0)
        stream->want_flush = true ;

    return reserved ;
}

static void sse_stream_io_callback(sse_watcher_t *w, void *data, int revents)
{
    sse_stream_t *stream = (sse_stream_t *)data ;

    if (!stream)
        return ;

    sse_stream_ref(stream) ;

    // Handle errors first
    if (revents & (SSE_ERROR | SSE_HUP)) {
        stream->error = w->api_errno ? w->api_errno : EIO ;
        stream->closed = true ;

        if (stream->error_cb) {
            stream->error_cb(stream, stream->error, stream->data) ;
        }

        if (stream->close_cb) {
            stream->close_cb(stream, stream->data) ;
        }

        sse_stream_unref(stream) ;
        return ;
    }

    // Handle read events
    if (revents & SSE_READ) {
        ssize_t result = sse_stream_read(stream) ;
        if (result < 0) {
            stream->error = errno ;
            if (stream->error_cb) {
                stream->error_cb(stream, stream->error, stream->data) ;
            }

            stream->closed = true;
            if (stream->close_cb) {
                stream->close_cb(stream, stream->data);
            }

        } else if (result == 0) {

            if (!error_isagain(errno)) {
                stream->closed = true;
                if (stream->close_cb) {
                    stream->close_cb(stream, stream->data);
                }
            }

        } else {
            // Data received
            if (stream->read_cb) {
                stream->read_cb(stream, stream->data) ;
            }
        }
    }

    // Handle write events
    if (revents & SSE_WRITE && stream->want_flush) {
        ssize_t result = sse_stream_flush(stream) ;
        if (result < 0) {
            stream->error = errno ;
            if (stream->error_cb) {
                stream->error_cb(stream, stream->error, stream->data) ;
            }

            stream->closed = true;
            if (stream->close_cb) {
                stream->close_cb(stream, stream->data);
            }
        } else if (result > 0) {
            // Data sent
            if (stream->write_cb) {
                stream->write_cb(stream, stream->data) ;
            }
        }
    }

    sse_stream_unref(stream) ;
}

void sse_stream_close(sse_stream_t *stream)
{
    if (!stream || stream->closed)
        return ;

    stream->closed = true ;
    sse_stop_io(&stream->watcher) ;
    sse_stream_unref(stream) ;
}

static sse_stream_t *sse_stream_ref(sse_stream_t *stream)
{
    if (!stream)
        return NULL ;
    stream->refcount++ ;
    return stream ;
}

static void sse_stream_unref(sse_stream_t *stream)
{
    if (!stream)
        return ;

    stream->refcount-- ;
    if (stream->refcount == 0)
        sse_stream_cleanup_internal(stream) ;
}

static void sse_stream_cleanup_internal(sse_stream_t *stream)
{
    int fd  = stream->fd ;
    stream->fd = -1 ;

    if (fd >= 0)
        close(fd) ;

    // Close any pending file descriptors
    close_fd_from_buffer(&stream->fdin.buf) ;
    close_fd_from_buffer(&stream->fdout.buf) ;

    sse_stop_io(&stream->watcher);

    stream->want_flush = false ;
    stream->closed = true ;
    stream->error = ECANCELED ; // optional: indicate shutdown reason

    // Optional: zero out callbacks
    stream->read_cb = NULL ;
    stream->write_cb = NULL ;
    stream->close_cb = NULL ;
    stream->error_cb = NULL ;
    stream->data = NULL ;
}