/*
 * stream_message.c
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
#include <sys/types.h>
#include <sys/uio.h>

#include <oblibs/sse_stream.h>
#include <oblibs/io_message.h>
#include <oblibs/stream_message.h>

int stream_message_reader_init(stream_message_t *reader, sse_stream_t *stream, stream_message_handler_t *handler, void *header_buf, size_t header_size, void *payload_buf, size_t payload_size, void *data)
{
    if (!reader || !stream || !handler)
       return (errno = EINVAL, 0) ;

    if (!header_buf || header_size == 0)
        return (errno = EINVAL, 0) ;

    if (!payload_buf || payload_size == 0)
        return (errno = EINVAL, 0) ;

    memset(reader, 0, sizeof(*reader)) ;

    reader->stream = stream ;
    reader->handler = handler ;
    reader->data = data ;

    // Initialize the io_message_reader with the stream's io_rb
    if (!io_message_reader_init(&reader->io_reader, &stream->io, header_buf, header_size, payload_buf, payload_size, data))
        return 0 ;

    return 1 ;
}

int stream_message_read(stream_message_t *reader, io_message_header_parser_t *parse_header)
{
    if (!reader || !parse_header)
        return (errno = EINVAL, -1) ;

    // Try to read and parse a message using io_message
    int result = io_message_read(&reader->io_reader, parse_header) ;

    if (result < 0)
        return -1 ;  // Error occurred

    if (result == 0)
        return 0 ;   // Need more data

    // We have a complete message
    void *header = NULL, *payload = NULL ;
    size_t header_len = 0, payload_len = 0 ;

    if (!io_message_get_buffers(&reader->io_reader, &header, &header_len, &payload, &payload_len))
        return (errno = EINVAL, -1) ;

    // Collect any file descriptors that arrived with this message
    int fds[SSE_STREAM_MAX_FDS] ;
    int nfd = 0 ;
    int fd ;

    while (nfd < SSE_STREAM_MAX_FDS && sse_stream_get_fd(reader->stream, &fd) > 0) {
        fds[nfd++] = fd ;
    }

    // Build the message structure
    struct iovec iov[2] ;
    int iovcnt = 1 ;

    iov[0].iov_base = header ;
    iov[0].iov_len = header_len ;

    if (payload && payload_len > 0) {
        iov[1].iov_base = payload ;
        iov[1].iov_len = payload_len ;
        iovcnt = 2 ;
    }

    io_rb_iovec_t msg = {
        .iov = iov,
        .niov = iovcnt,
        .afd = (nfd > 0) ? fds : NULL,
        .nfd = nfd,
    };

    // Call the handler
    if (reader->handler)
        reader->handler(&msg, reader->data) ;

    // Reset for next message
    io_message_reset(&reader->io_reader) ;

    return 1;
}

int stream_message_send(sse_stream_t *stream, const void *header, size_t header_len, const void *payload, size_t payload_len)
{
    if (!stream || !header || !header_len)
        return (errno = EINVAL, -1) ;

    struct iovec iov[2];
    int iovcnt = 1;

    iov[0].iov_base = (void *)header;
    iov[0].iov_len = header_len;

    if (payload && payload_len > 0) {
        iov[1].iov_base = (void *)payload;
        iov[1].iov_len = payload_len;
        iovcnt = 2;
    }

    return sse_stream_putv(stream, iov, iovcnt);
}

int stream_message_send_with_fds(sse_stream_t *stream, const void *header, size_t header_len, const void *payload, size_t payload_len, const int *fds, int nfd)
{
    if (!stream || !header || header_len == 0)
        return (errno = EINVAL, -1);

    if (nfd < 0)
        return (errno = EINVAL, -1) ;

    if (nfd > 0 && !fds)
        return (errno = EINVAL, -1) ;

    // Validate file descriptors
    for (int i = 0; i < nfd; i++) {
        if (fds[i] < 0)
        return (errno = EBADF, -1) ;
    }

    struct iovec iov[2];
    int iovcnt = 1;

    iov[0].iov_base = (void *)header ;
    iov[0].iov_len = header_len ;

    if (payload && payload_len > 0) {
        iov[1].iov_base = (void *)payload ;
        iov[1].iov_len = payload_len ;
        iovcnt = 2 ;
    }

    return sse_stream_putv_fd(stream, iov, iovcnt, fds, nfd) ;
}

int stream_message_sendv(sse_stream_t *stream, const struct iovec *iov, int iovcnt)
{
    if (!stream || !iov || iovcnt <= 0)
        return (errno = EINVAL, -1);

    return sse_stream_putv(stream, iov, iovcnt);
}

int stream_message_sendv_with_fds(sse_stream_t *stream, const struct iovec *iov, int iovcnt, const int *fds, int nfd)
{
    if (!stream || !iov || iovcnt <= 0)
        return (errno = EINVAL, -1) ;

    if (nfd < 0)
        return (errno = EINVAL, -1) ;

    if (nfd > 0 && !fds)
        return (errno = EINVAL, -1) ;

    // Validate file descriptors
    for (int i = 0; i < nfd; i++) {
        if (fds[i] < 0)
        return (errno = EBADF, -1) ;
    }

    return sse_stream_putv_fd(stream, iov, iovcnt, fds, nfd) ;
}
