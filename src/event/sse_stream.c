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

#include <oblibs/rbuffer.h>
#include <oblibs/socket.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/io_rb.h>

// Forward declaration of internal callback
static void sse_stream_io_callback(sse_watcher_t *w, void *data, int revents) ;
static void sse_stream_cleanup_internal(sse_stream_t *stream) ;
static sse_stream_t *sse_stream_ref(sse_stream_t *stream) ;
static void sse_stream_unref(sse_stream_t *stream) ;

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

int sse_stream_init(sse_stream_t *stream, int fd, sse_stream_cb_t *read_cb, sse_stream_cb_t *write_cb, sse_stream_cb_t *close_cb, sse_stream_error_cb_t *error_cb, void *data)
{
    if (!stream || fd < 0)
        return (errno = EINVAL, 0) ;

    memset(stream, 0, sizeof(*stream)) ;

    // Initialize io_rb with NULL ops to use default socket I/O
    if (!io_rb_init(&stream->io, NULL, fd))
        return 0 ;

    stream->fd = fd ;
    stream->closed = false ;
    stream->error = 0 ;

    stream->read_cb = read_cb ;
    stream->write_cb = write_cb ;
    stream->close_cb = close_cb ;
    stream->error_cb = error_cb ;
    stream->data = data ;

    return 1 ;
}

sse_stream_t *sse_stream_new(int fd, sse_stream_cb_t *read_cb, sse_stream_cb_t *write_cb, sse_stream_cb_t *close_cb, sse_stream_error_cb_t *error_cb, void *data)
{
    sse_stream_t *stream = malloc(sizeof(*stream)) ;
    if (!stream)
        return NULL ;

    if (!sse_stream_init(stream, fd, read_cb, write_cb, close_cb, error_cb, data)) {
        free(stream) ;
        return NULL ;
    }

    return stream ;
}

int sse_stream_attach(sse_stream_t *stream, sse_epoll_t *p, int priority)
{
    if (!stream || !p)
        return (errno = EINVAL, 0) ;

    return sse_start_io(p, &stream->watcher, sse_stream_io_callback, stream, stream->fd, SSE_READ, priority) ;
}

int sse_stream_detach(sse_stream_t *stream)
{
    if (!stream)
        return (errno = EINVAL, 0) ;

    return sse_stop_io(&stream->watcher) ;
}

void sse_stream_modify(sse_stream_t *stream)
{
    if (!stream || stream->closed)
        return ;

    int events = SSE_READ ;  // Always monitor for reads

    if (stream->io.want_flush)
        events |= SSE_WRITE ;  // Only monitor writes if we have data

    sse_modify_io(&stream->watcher, stream->fd, events) ;
}

ssize_t sse_stream_read(sse_stream_t *stream)
{
    if (!stream || stream->closed)
        return (errno = EBADF, -1) ;

    return io_rb_read(&stream->io, stream->data) ;
}

ssize_t sse_stream_flush(sse_stream_t *stream)
{
    if (!stream || stream->closed)
        return (errno = EBADF, -1) ;

    ssize_t r = io_rb_flush(&stream->io, stream->data) ;

    sse_stream_modify(stream) ;

    return r ;
}

int sse_stream_put(sse_stream_t *stream, const void *data, size_t count)
{
    if (!stream || !data || stream->closed)
        return (errno = EINVAL, -1) ;

    if (io_rb_put(&stream->io, data, count, stream->data) < 0)
        return -1 ;

    sse_stream_modify(stream) ;

    return 0 ;
}

int sse_stream_putv(sse_stream_t *stream, const struct iovec *iov, int iovcnt)
{
    if (!stream || !iov || iovcnt <= 0 || stream->closed)
        return (errno = EINVAL, -1) ;

    if (io_rb_putv(&stream->io, iov, iovcnt, stream->data) < 0)
        return -1 ;

    sse_stream_modify(stream) ;

    return 0 ;
}

ssize_t sse_stream_get(sse_stream_t *stream, void *dst, size_t count)
{
    if (!stream || !dst || stream->closed)
        return (errno = EINVAL, -1) ;

    return io_rb_get(&stream->io, dst, count) ;
}

int sse_stream_put_fd(sse_stream_t *stream, int fd)
{
    if (!stream || fd < 0 || stream->closed)
        return (errno = EINVAL, -1) ;

    if (io_rb_put_fd(&stream->io, fd) < 0)
        return -1 ;

    sse_stream_modify(stream) ;

    return 0 ;
}

int sse_stream_putv_fd(sse_stream_t *stream, const struct iovec *iov, int iovcnt, const int *fds, int nfd)
{
    if (!stream || !iov || iovcnt <= 0 || nfd < 0 || stream->closed)
        return (errno = EINVAL, -1) ;

    if (io_rb_putv_fd(&stream->io, iov, iovcnt, fds, nfd, stream->data) < 0)
        return -1 ;

    sse_stream_modify(stream) ;

    return 0 ;
}

ssize_t sse_stream_get_fd(sse_stream_t *stream, int *fd)
{
    if (!stream || !fd || stream->closed)
        return (errno = EINVAL, -1) ;

    return io_rb_get_fd(&stream->io, fd);
}

void sse_stream_close_fds(sse_stream_t *stream)
{
    if (!stream)
        return ;

    io_rb_close_fds(&stream->io.fdin.buf) ;
    io_rb_close_fds(&stream->io.fdout.buf) ;
}

size_t sse_stream_pending_in(const sse_stream_t *stream)
{
    return io_rb_pending_in(&stream->io) ;
}

size_t sse_stream_pending_out(const sse_stream_t *stream)
{
    return io_rb_pending_out(&stream->io) ;
}

size_t sse_stream_pending_fdin(const sse_stream_t *stream)
{
    return io_rb_pending_fdin(&stream->io) ;
}

size_t sse_stream_pending_fdout(const sse_stream_t *stream)
{
    return io_rb_pending_fdout(&stream->io) ; ;
}

bool sse_stream_is_closed(const sse_stream_t *stream)
{
    return stream ? stream->closed : true ;
}

int sse_stream_get_error(const sse_stream_t *stream)
{
    return stream ? stream->error : EINVAL ;
}

static void sse_stream_io_callback(sse_watcher_t *w, void *data, int revents)
{
    sse_stream_t *stream = (sse_stream_t *)data;

    /* Ignore a stream already closed: when a callback closes a *sibling* stream,
     * that stream's watcher is removed from epoll but stays in sse's pending
     * queue for this cycle, so the dispatch loop re-enters here. Without this
     * guard we would ref/unref it again, re-run cleanup, and double-defer its
     * free. The stream memory is still valid (its free is deferred). */
    if (!stream || stream->closed)
        return;

    sse_stream_ref(stream);

    // Handle errors first
    if (revents & (SSE_ERROR | SSE_HUP)) {

        if (revents & SSE_ERROR) {

            stream->error = w->api_errno ? w->api_errno : EIO ;
            if (stream->error_cb)
                stream->error_cb(stream, stream->error, stream->data) ;

        } else {
            stream->error = 0 ;  // Clean close
        }

        if (stream->close_cb)
            stream->close_cb(stream, stream->data) ;

        sse_stream_unref(stream) ;
        return ;
    }

    // Handle read events
    if (revents & SSE_READ) {
        ssize_t result = sse_stream_read(stream);
        if (result < 0) {
            stream->error = errno ;

            if (stream->error_cb)
                stream->error_cb(stream, stream->error, stream->data) ;

            if (stream->close_cb)
                stream->close_cb(stream, stream->data) ;

        } else if (result == 0) {

            if (!error_isagain(errno)) {
                if (stream->close_cb)
                    stream->close_cb(stream, stream->data) ;
            }
        } else {
            // Normal data processing after handshake
            if (stream->read_cb)
                stream->read_cb(stream, stream->data) ;
        }
    }

    // Handle write events
    if (revents & SSE_WRITE && stream->io.want_flush) {
        ssize_t result = sse_stream_flush(stream) ;
        if (result < 0) {
            stream->error = errno ;
            if (stream->error_cb)
                stream->error_cb(stream, stream->error, stream->data) ;

            if (stream->close_cb)
                stream->close_cb(stream, stream->data);

        } else if (result > 0) {
            // Data sent
            if (stream->write_cb)
                stream->write_cb(stream, stream->data) ;
        }
    }

    sse_stream_unref(stream) ;
}

void sse_stream_close(sse_stream_t *stream)
{
    if (!stream)
        return ;

    if (!stream->closed)
        stream->closed = true ;

    sse_stream_unref(stream) ;
}

static sse_stream_t *sse_stream_ref(sse_stream_t *stream)
{
    if (!stream)
        return NULL ;
    io_rb_ref(&stream->io) ;
    return stream ;
}

static void sse_stream_unref(sse_stream_t *stream)
{
    if (!stream)
        return ;

    io_rb_unref(&stream->io) ;
    if (!stream->io.refcount)
        sse_stream_cleanup_internal(stream) ;
}

static void sse_stream_cleanup_internal(sse_stream_t *stream)
{
    if (!stream)
        return ;
    int fd  = stream->fd ;
    sse_epoll_t *p = stream->watcher.p ; // capture before sse_free_io

    sse_free_io(&stream->watcher) ;

    // Close any pending file descriptors
    sse_stream_close_fds(stream) ;

    stream->closed = true ;
    stream->error = ECANCELED ; // optional: indicate shutdown reason

    // Optional: zero out callbacks
    stream->read_cb = NULL ;
    stream->write_cb = NULL ;
    stream->close_cb = NULL ;
    stream->error_cb = NULL ;
    stream->data = NULL ;
    stream->fd = -1 ;

    /* Defer the free until the current dispatch has fully returned: closing a
     * stream from another stream's callback (sibling-free) would otherwise free
     * a watcher still sitting in sse's pending queue. Fall back to an immediate
     * free if the loop is unknown or the deferral cannot be registered (OOM). */
    if (!p || !sse_defer(p, free, stream))
        free(stream) ;

    if (fd >= 0)
        close(fd) ;
}