/*
 * sse_stream.h
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
 *
 * @brief SSE-integrated stream I/O handler with ring buffering and FD passing.
 *
 * Provides a high-level abstraction for non-blocking stream sockets (especially
 * Unix domain sockets), integrated with the SSE event loop. Supports zero-copy
 * data buffering via rbuffer and file descriptor passing using SCM_RIGHTS.
 *
 * Designed for full-duplex communication where both data and file descriptors
 * are exchanged safely and efficiently.
 */

#ifndef SSE_STREAM_H
#define SSE_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <oblibs/rbuffer.h>
#include <oblibs/socket.h>
#include <oblibs/sse.h>

#define STREAM_BUFFER_SIZE 4096 // maybe we should use (64 * 1024)?

#ifndef MAX_FDS
#define MAX_FDS 253  // SCM_MAX_FD on Linux
#endif

struct sse_stream_buffer_s
{
    rbuffer_t buf ;
    uint8_t data[STREAM_BUFFER_SIZE] ;
} ;
typedef struct sse_stream_buffer_s sse_stream_buffer_t ;

/**
 * @struct sse_stream_s
 * @brief Connection structure with SSE integration and rbuffer-based I/O
 *
 * Manages a socket connection with event-driven I/O, buffering for both
 * regular data and file descriptors, and integration with the SSE event loop.
 */
typedef struct sse_stream_s sse_stream_t ;
typedef void (sse_stream_cb_t)(sse_stream_t *stream, void *data) ;
typedef void (sse_stream_error_cb_t)(sse_stream_t *stream, int error, void *data) ;

struct sse_stream_s
{
    int fd ; // Socket file descriptor
    sse_watcher_t watcher ; // SSE I/O watcher for this connection

    // Input buffering
    sse_stream_buffer_t in, out ; // in/out data buffer
    sse_stream_buffer_t fdin, fdout ; // in/out fd buffer

    bool want_flush ; // Flag indicating pending output
    bool closed ; // Connection closed flag
    int error ; // Last error code
    int refcount ;

    // User data and callbacks
    void *data ; // User-provided data
    sse_stream_cb_t *read_cb, *write_cb, *close_cb ;
    sse_stream_error_cb_t *error_cb ;
} ;

// Connection lifecycle
/**
 * @brief Initialize a stream on an existing socket.
 *
 * Sets up internal buffers and associates callbacks. The socket must already be
 * in non-blocking mode. This function does not take ownership of the fd.
 *
 * @param stream    Pointer to pre-allocated sse_stream_t
 * @param fd        Open socket file descriptor (must be O_NONBLOCK)
 * @param read_cb   Called when data is available to read
 * @param write_cb  Called after successful flush of pending output
 * @param close_cb  Called when connection closes cleanly
 * @param error_cb  Called on I/O errors (including EPIPE, ECONNRESET)
 * @param data      User-defined context passed to all callbacks
 *
 * @return 1 on success, 0 on failure
 * @retval 0 errno is set
 *
 * @errno EINVAL if stream is NULL or fd < 0
 *
 * @note Safe to call multiple times only if re-initialized properly.
 *       Must be paired with sse_stream_close() to avoid resource leaks.
 */
extern int sse_stream_init(sse_stream_t *stream, int fd, sse_stream_cb_t *read_cb, sse_stream_cb_t *write_cb, sse_stream_cb_t *close_cb, sse_stream_error_cb_t *error_cb, void *data) ;

/**
 * @brief Attach the stream to an SSE event loop.
 *
 * Starts monitoring the socket for read/write events based on current state.
 * Automatically enables POLLIN if input buffer has space, POLLOUT if output
 * pending.
 *
 * @param stream    Initialized stream
 * @param p         SSE event loop instance
 * @param priority  Event dispatch priority (higher = earlier)
 *
 * @return 1 on success, 0 on failure
 * @retval 0 errno is set
 *
 * @errno EINVAL if stream or p is NULL
 *
 * @note No effect if already attached. Use sse_stream_detach() to reattach.
 */
extern int sse_stream_attach(sse_stream_t *stream, sse_epoll_t *p, int priority) ;

/**
 * @brief Detach the stream from the event loop.
 *
 * Stops I/O monitoring. Safe to call even if not attached.
 *
 * @param stream Stream to detach
 *
 * @return 1 if detached, 0 if already stopped or invalid
 * @retval 0 errno may be set
 *
 * @errno EINVAL if stream is NULL
 *
 * @note Does NOT close the stream or free resources. Call sse_stream_close().
 */
extern int sse_stream_detach(sse_stream_t *stream) ;

/**
 * @brief Close the stream gracefully.
 *
 * Shuts down I/O, detaches from event loop, schedules cleanup. If reference
 * count allows, destroys internal state and closes the socket.
 *
 * @param stream Stream to close
 *
 * @note Safe to call multiple times. Has no effect after first call.
 *       Triggers close/error callbacks unless already closed.
 *       Always decrements refcount; final cleanup happens asynchronously.
 */
extern void sse_stream_close(sse_stream_t *stream) ;

// Data I/O operations
/**
 * @brief Read available data from peer into internal buffer.
 *
 * Uses recvmsg() with MSG_DONTWAIT. Handles EAGAIN/EWOULDBLOCK by returning 0
 * with errno unchanged (typically EAGAIN or EWOULDBLOCK).
 * On graceful EOF (recvmsg returns 0), sets errno = 0 and returns 0.
 *
 * @param stream Stream to read from
 *
 * @return Number of bytes received, 0 for transient/EOF, -1 on error
 * @retval >0  Successfully read and queued @c n bytes of data
 * @retval 0   Either:
 *             - No data available now (EAGAIN/EWOULDBLOCK, errno preserved), OR
 *             - Peer performed orderly shutdown (EOF, errno explicitly set to 0)
 * @retval -1  Fatal error occurred (inspect errno)
 *
 * @errno EBADF      if stream is NULL or already closed
 * @errno EOVERFLOW  if input buffer is full
 * @errno (various)  if recvmsg() or ancillary data processing failed (e.g., ECONNRESET, EIO, EOVERFLOW from decode_cmsg)
 *
 * @note
 * - Caller must distinguish EOF from EAGAIN by checking errno == 0 after return value 0.
 * - Does not invoke callbacks — purely a data pump.
 * - Ancillary FDs are extracted and stored in fdin buffer even on EOF.
 * - Buffer space is required — returns EOVERFLOW if full.
 */
extern ssize_t sse_stream_read(sse_stream_t *stream);

/**
 * @brief Flush pending output to socket.
 *
 * Sends buffered data and queued file descriptors via sendmsg(). Stops when
 * kernel write queue is full or buffer is empty.
 *
 * @param stream Stream to flush
 *
 * @return Total bytes sent (>0), 0 if blocked, -1 on fatal error
 * @retval >0 Some data successfully sent
 * @retval 0  Would block — try again when writable
 * @retval -1 Permanent error (check errno)
 *
 * @errno EBADF if stream is NULL or closed
 *
 * @note Returns 0 on EAGAIN/EWOULDBLOCK. Never sets errno to EAGAIN.
 *       Advances buffers only on success. Clears want_flush if fully drained.
 */
extern ssize_t sse_stream_flush(sse_stream_t *stream) ;

/**
 * @brief Queue data for transmission.
 *
 * Copies data into output ring buffer. Attempts partial flush if insufficient
 * space. May fail with EAGAIN even after flush if still no room.
 *
 * @param stream Stream to write to
 * @param data   Buffer to send (must be valid)
 * @param count  Number of bytes to queue
 *
 * @return 0 on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if stream, data invalid or closed
 * @errno EAGAIN if insufficient buffer space after attempted flush
 * @errno EOVERFLOW if single write exceeds buffer capacity
 *
 * @note Does NOT guarantee delivery — only queuing. Call flush manually or wait
 *       for write event. Increments want_flush automatically.
 */
extern int sse_stream_put(sse_stream_t *stream, const void *data, size_t count) ;

/**
 * @brief Read data from input buffer.
 *
 * Extracts exactly `count` bytes. Fails immediately if less than `count` bytes
 * are available.
 *
 * @param stream Stream to read from
 * @param dst    Destination buffer
 * @param count  Number of bytes to extract
 *
 * @return count on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if stream or dst is NULL or closed
 * @errno EAGAIN if fewer than `count` bytes available
 *
 * @note Does NOT advance buffer — use rbuffer_read internally which does.
 *       Use sse_stream_restore() to un-read data.
 */
extern ssize_t sse_stream_get(sse_stream_t *stream, void *dst, size_t count) ;

// File descriptor operations
/**
 * @brief Queue a file descriptor for sending.
 *
 * Will be transmitted in next control message (SCM_RIGHTS). Requires at least
 * sizeof(int) free space in fdout buffer.
 *
 * @param stream Stream to send over
 * @param fd     Valid open file descriptor
 *
 * @return 0 on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if stream is NULL, closed, or fd < 0
 * @errno EOVERFLOW if no space in fdout buffer
 *
 * @note FD is consumed on successful put — caller loses ownership.
 *       Sent during next sse_stream_flush().
 */
extern int sse_stream_put_fd(sse_stream_t *stream, int fd) ;

/**
 * @brief Receive one queued file descriptor.
 *
 * Extracts first FD from incoming fd buffer.
 *
 * @param stream Stream to receive from
 * @param fd     Pointer to store received FD
 *
 * @return sizeof(int) on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if stream or fd is NULL or closed
 * @errno EAGAIN if no FD available
 *
 * @note Caller assumes ownership of returned FD. Must close it.
 */
extern ssize_t sse_stream_get_fd(sse_stream_t *stream, int *fd) ;

/**
 * @brief Close all pending inbound/outbound file descriptors.
 *
 * Closes any FDs waiting to be sent or received. Useful during cleanup to avoid
 * leaks. Safe to call at any time.
 *
 * @param stream Stream whose FD queues to clear
 *
 * @note Does NOT affect currently held FDs outside buffers.
 *       Idempotent — safe to call repeatedly.
 */
extern void sse_stream_close_fds(sse_stream_t *stream) ;

// Status queries
/**
 * @brief Get number of bytes available to read.
 *
 * @param stream Stream to query
 *
 * @return Byte count, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_in(const sse_stream_t *stream) ;

/**
 * @brief Get number of bytes pending for write.
 *
 * @param stream Stream to query
 *
 * @return Byte count, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_out(const sse_stream_t *stream) ;

/**
 * @brief Get number of file descriptors waiting to be read.
 *
 * @param stream Stream to query
 *
 * @return Count of FDs, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_fdin(const sse_stream_t *stream) ;

/**
 * @brief Get number of file descriptors waiting to be sent.
 *
 * @param stream Stream to query
 *
 * @return Count of FDs, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_fdout(const sse_stream_t *stream) ;

/**
 * @brief Check if stream is closed.
 *
 * Includes both user-initiated close and error-induced closure.
 *
 * @param stream Stream to check
 *
 * @return true if closed, false otherwise
 *
 * @note Returns true even if close is pending due to refcounting.
 */
extern bool sse_stream_is_closed(const sse_stream_t *stream) ;

/**
 * @brief Get last error code.
 *
 * Valid only if stream is closed due to error.
 *
 * @param stream Stream to query
 *
 * @return Error code (e.g., EPIPE, ECONNRESET), or EINVAL if invalid
 *
 * @note Returns ECANCELED if closed via sse_stream_close().
 */
extern int sse_stream_get_error(const sse_stream_t *stream) ;

// Buffer control
/**
 * @brief Rewind input buffer by given number of bytes.
 *
 * Allows re-processing of previously read data. Safe as long as count ≤ used.
 *
 * @param stream Stream whose input buffer to rewind
 * @param count  Number of bytes to un-read
 *
 * @note No effect if count > rbuffer_used(&stream->in.buf).
 *       Does not restore file descriptors — only data.
 */
extern void sse_stream_restore(sse_stream_t *stream, size_t count) ;

/**
 * @brief Reserve space in output buffer without writing.
 *
 * Useful for protocols requiring header/body layout (reserve header, fill later).
 *
 * @param stream Stream to reserve in
 * @param count  Number of bytes to reserve
 *
 * @return Reserved amount (≤ count), or 0 on error
 * @retval 0 errno is set
 *
 * @errno EINVAL if stream is NULL or closed
 *
 * @note Increments want_flush. Space remains reserved until written.
 */
extern int sse_stream_reserve_output(sse_stream_t *stream, size_t count) ;

// Convenience functions for Unix domain sockets
/**
 * @brief Create a Unix domain server socket.
 *
 * Binds and listens on specified path. Socket is non-blocking and CLOEXEC.
 *
 * @param path    Null-terminated path to bind
 * @param backlog Listen backlog (use 0 for default)
 *
 * @return File descriptor on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if path is NULL
 * @errno EADDRINUSE if path already bound
 *
 * @note Unlinks path on creation — suitable for transient services.
 *       Caller must unlink manually on shutdown if desired.
 */
extern int sse_streamux_create_server(const char *path, int backlog) ;

/**
 * @brief Connect to a Unix domain server.
 *
 * Establishes non-blocking, CLOEXEC connection.
 *
 * @param path Server path to connect to
 *
 * @return Connected socket fd on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if path is NULL
 * @errno ENOENT if path does not exist
 *
 * @note Connection may be in progress (EINPROGRESS) — use with event loop.
 */
extern int sse_streamux_create_client(const char *path) ;

/**
 * @brief Accept a new connection from a Unix server socket.
 *
 * Returns non-blocking, CLOEXEC client socket.
 *
 * @param fd Listening server socket
 *
 * @return Client fd on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if fd < 0
 *
 * @note Always returns non-blocking socket regardless of listening socket flags.
 */
extern int sse_streamux_accept(int fd) ;

#endif