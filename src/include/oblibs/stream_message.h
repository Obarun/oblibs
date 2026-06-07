/*
 * stream_message.h
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
 *
 * @brief Message-oriented stream I/O abstraction built on io_message and sse_stream.
 *
 * This module provides a high-level API for sending and receiving structured messages
 * over SSE streams. It integrates io_message's parsing capabilities with sse_stream's
 * transport layer, supporting:
 * - Variable-length headers and payloads
 * - File descriptor passing via SCM_RIGHTS
 * - Non-blocking incremental message parsing
 * - Protocol-agnostic message framing
 *
 * The protocol is defined by providing a header parser callback that extracts the
 * payload length from received header bytes.
 */

#ifndef OB_STREAM_MESSAGE_H
#define OB_STREAM_MESSAGE_H

#include <sys/types.h>
#include <sys/uio.h>

#include <oblibs/sse_stream.h>
#include <oblibs/io_message.h>
#include <oblibs/io_rb.h>

/**
 * @brief Callback invoked when a complete message has been received.
 *
 * @param msg   Pointer to the assembled message
 * @param data  User-provided context data
 *
 * @note The message buffers and file descriptors are only valid during this callback.
 *       If you need to keep the FDs, you must not close them ; the reader doesn't own them
 *       after the callback returns. If you don't consume the FDs, they will leak.
 */
typedef void (stream_message_handler_t)(io_rb_iovec_t *msg, void *data) ;

/**
 * @brief Message reader that wraps io_message_t for stream integration.
 *
 * Manages the state machine for parsing messages from a stream and collecting
 * any associated file descriptors.
 */
struct stream_message_s
{
    sse_stream_t *stream ;               // Associated stream for I/O
    io_message_t io_reader ;     // Underlying message parser
    stream_message_handler_t *handler ;  // Callback for complete messages
    void *data ;                         // User context data
} ;
typedef struct stream_message_s stream_message_t ;

/**
 * @brief Initialize a message reader for a stream.
 *
 * Sets up the reader to parse messages from the given stream using external buffers.
 * The stream must already be initialized and attached to an event loop.
 *
 * @param reader        Pointer to reader structure to initialize
 * @param stream        Associated stream (must remain valid for reader lifetime)
 * @param handler       Callback invoked for each complete message
 * @param header_buf    Buffer to store message headers (must be at least header_size bytes)
 * @param header_size   Maximum header size in bytes (must be > 0)
 * @param payload_buf   Buffer to store message payloads (must be at least payload_size bytes)
 * @param payload_size  Maximum payload size in bytes (must be > 0)
 * @param data          User context passed to handler callback
 *
 * @return 1 on success, 0 on failure
 * @retval 0 errno is set to EINVAL for invalid parameters
 *
 * @note The buffers must remain valid for the entire lifetime of the reader.
 *       The reader does not take ownership of these buffers.
 */
extern int stream_message_reader_init(stream_message_t *reader, sse_stream_t *stream, stream_message_handler_t *handler, void *header_buf, size_t header_size, void *payload_buf, size_t payload_size, void *data) ;

/**
 * @brief Attempt to read and parse one complete message from the stream.
 *
 * This function should be called from the stream's read callback whenever data is available.
 * It processes as much buffered data as possible and invokes the handler callback when a
 * complete message is assembled.
 *
 * @param reader        Initialized message reader
 * @param parse_header  Callback to parse header and determine payload length
 *                      (see io_message_header_parser_t in io_message.h)
 *
 * @return Status code:
 *   - 1: Complete message was parsed and handler was invoked
 *   - 0: Need more data (incomplete message)
 *   - -1: Error occurred (errno is set)
 *
 * @errno EINVAL     Invalid parameters
 * @errno EPROTO     Protocol error (malformed header)
 * @errno EMSGSIZE   Payload exceeds buffer capacity
 * @errno EOVERFLOW  Header size exceeded
 *
 * @note This function can be called repeatedly as data arrives. It maintains state
 *       between calls to handle partial messages spanning multiple read events.
 * @note File descriptors received with the message are passed to the handler. The
 *       handler is responsible for closing them if not needed.
 */
extern int stream_message_read(stream_message_t *reader, io_message_header_parser_t *parse_header) ;

/**
 * @brief Send a message with header and optional payload (no file descriptors).
 *
 * Queues the message for transmission. The actual send happens during the next
 * flush operation (typically triggered by the event loop).
 *
 * @param stream        Target stream
 * @param header        Header data (must not be NULL)
 * @param header_len    Header size in bytes (must be > 0)
 * @param payload       Payload data (may be NULL if payload_len is 0)
 * @param payload_len   Payload size in bytes (may be 0)
 *
 * @return 0 on success, -1 on failure
 * @retval -1 errno is set
 *
 * @errno EINVAL    Invalid parameters
 * @errno EAGAIN    Insufficient buffer space (try again after flush)
 * @errno EOVERFLOW Message size exceeds buffer capacity
 *
 * @note The buffers are copied immediately, so they can be freed after this call returns.
 */
extern int stream_message_send(sse_stream_t *stream, const void *header, size_t header_len, const void *payload, size_t payload_len) ;

/**
 * @brief Send a message with header, optional payload, and file descriptors.
 *
 * Queues the message and associated file descriptors for transmission using SCM_RIGHTS.
 *
 * @param stream        Target stream
 * @param header        Header data (must not be NULL)
 * @param header_len    Header size in bytes (must be > 0)
 * @param payload       Payload data (may be NULL if payload_len is 0)
 * @param payload_len   Payload size in bytes (may be 0)
 * @param fds           Array of file descriptors to send (may be NULL if nfd is 0)
 * @param nfd           Number of file descriptors (must be >= 0)
 *
 * @return 0 on success, -1 on failure
 * @retval -1 errno is set
 *
 * @errno EINVAL    Invalid parameters
 * @errno EBADF     One or more FDs are invalid (< 0)
 * @errno EAGAIN    Insufficient buffer space (try again after flush)
 * @errno EOVERFLOW Message or FD count exceeds buffer capacity
 *
 * @note The buffers are copied immediately. File descriptors are queued but not duplicated.
 *       The caller retains ownership and should close them after successful send if desired.
 */
extern int stream_message_send_with_fds(sse_stream_t *stream, const void *header, size_t header_len, const void *payload, size_t payload_len, const int *fds, int nfd) ;

/**
 * @brief Send a message using vectored I/O (no file descriptors).
 *
 * Allows sending a message composed of multiple non-contiguous buffers without copying.
 *
 * @param stream    Target stream
 * @param iov       Array of iovec structures
 * @param iovcnt    Number of iovec entries (must be > 0)
 *
 * @return 0 on success, -1 on failure
 * @retval -1 errno is set
 *
 * @errno EINVAL    Invalid parameters
 * @errno EAGAIN    Insufficient buffer space
 * @errno EOVERFLOW Total message size exceeds buffer capacity
 */
extern int stream_message_sendv(sse_stream_t *stream, const struct iovec *iov, int iovcnt) ;

/**
 * @brief Send a message using vectored I/O with file descriptors.
 *
 * Combines vectored data transmission with file descriptor passing.
 *
 * @param stream    Target stream
 * @param iov       Array of iovec structures
 * @param iovcnt    Number of iovec entries (must be > 0)
 * @param fds       Array of file descriptors to send (may be NULL if nfd is 0)
 * @param nfd       Number of file descriptors (must be >= 0)
 *
 * @return 0 on success, -1 on failure
 * @retval -1 errno is set
 *
 * @errno EINVAL    Invalid parameters
 * @errno EBADF     One or more FDs are invalid
 * @errno EAGAIN    Insufficient buffer space
 * @errno EOVERFLOW Total message or FD count exceeds buffer capacity
 */
extern int stream_message_sendv_with_fds(sse_stream_t *stream, const struct iovec *iov, int iovcnt, const int *fds, int nfd) ;

#endif