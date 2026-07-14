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
 * @brief SSE-integrated stream I/O handler with ring buffering and FD passing.
 *
 * Provides a high-level abstraction for non-blocking stream sockets (especially
 * Unix domain sockets), integrated with the SSE event loop. Supports zero-copy
 * data buffering via rbuffer and file descriptor passing using SCM_RIGHTS.
 *
 * Designed for full-duplex communication where both data and file descriptors
 * are exchanged safely and efficiently.
 */

#ifndef OB_SSE_STREAM_H
#define OB_SSE_STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/uio.h>
#include <sys/types.h>

#include <oblibs/sse.h>
#include <oblibs/io_rb.h>

#define SSE_STREAM_MAX_FDS 253 // SCM_MAX_FD on Linux

/**
 * @struct sse_stream_s
 * @brief Connection structure with SSE integration and rbuffer-based I/O
 *
 * Manages a socket connection with event-driven I/O, buffering for both
 * regular data and file descriptors, and integration with the SSE event loop.
 * Uses an internal io_rb_t object for buffering and I/O operations.
 */
typedef struct sse_stream_s sse_stream_t ;
/**
 * @brief Callback type for read, write, and close events.
 *
 * Invoked by the SSE event loop when specific I/O events occur.
 *
 * @param stream The SSE stream instance triggering the event.
 * @param data   User-provided data associated with the stream.
 */
typedef void (sse_stream_cb_t)(sse_stream_t *stream, void *data) ;
/**
 * @brief Callback type for error events.
 *
 * Invoked by the SSE event loop when an I/O error occurs.
 *
 * @param stream The SSE stream instance triggering the event.
 * @param error  The error code encountered (e.g., EPIPE, ECONNRESET).
 * @param data   User-provided data associated with the stream.
 */
typedef void (sse_stream_error_cb_t)(sse_stream_t *stream, int error, void *data) ;

struct sse_stream_s
{
    int fd ;                     // Socket file descriptor
    sse_watcher_t watcher ;      // SSE I/O watcher for this connection

    bool closed ;                // Connection closed flag
    int error ;                  // Last error code

    // User data and callbacks
    void *data ;                 // User-provided context passed to all callbacks
    sse_stream_cb_t *read_cb, *write_cb, *close_cb ;
    sse_stream_error_cb_t *error_cb ;

    // LAST: io_rb_t ends with ~20 kB of backing arrays. Keeping io last puts
    // that block at the very tail of sse_stream_t, so no touched field ever
    // lands on its lazily-faulted pages. Any field added MUST go ABOVE io.
    io_rb_t io ;                 // I/O ring buffer (handles both data and FDs)
} ;

// Connection lifecycle
/**
 * @brief Initialize a stream on an existing socket.
 *
 * Sets up internal buffers and associates callbacks. The socket must already be
 * in non-blocking mode. This function does not take ownership of the fd.
 * Initializes the internal io_rb_t using default socket operations (NULL ops).
 * Sets the initial reference count to 1 via io_rb_init.
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
 * @brief Creates and initializes a new SSE stream.
 *
 * Allocates memory for a new sse_stream_t structure and initializes it with the provided
 * file descriptor and callback functions. If initialization fails, the allocated memory
 * is freed and NULL is returned.
 * The sse_stream_t should not be embedded by the caller, this API handles it internally.
 *
 * @param fd The file descriptor associated with the stream
 * @param read_cb Callback function invoked when data is available for reading
 * @param write_cb Callback function invoked when the stream is ready for writing
 * @param close_cb Callback function invoked when the stream is closed
 * @param error_cb Callback function invoked when an error occurs on the stream
 * @param data User-defined data pointer that will be passed to all callback functions
 *
 * @return A pointer to the newly created sse_stream_t on success, or NULL on failure
 *         (either due to memory allocation failure or initialization failure)
 */
extern sse_stream_t *sse_stream_new(int fd, sse_stream_cb_t *read_cb, sse_stream_cb_t *write_cb, sse_stream_cb_t *close_cb, sse_stream_error_cb_t *error_cb, void *data) ;

/**
 * @brief Attach the stream to an SSE event loop.
 *
 * Starts monitoring the socket for I/O events based on current state.
 * Initially registers for SSE_READ events. Automatically enables SSE_WRITE
 * events if output is pending (`io.want_flush` is true).
 * Uses the internal SSE watcher to manage events.
 *
 * @param stream    Initialized stream
 * @param p         SSE event loop instance
 * @param priority  Event dispatch priority (lower number = higher priority)
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
 * Stops I/O monitoring using the internal SSE watcher. Safe to call even if not attached.
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
 * Marks the stream as closed (`closed = true`). Decrements the internal reference count.
 * If the reference count reaches zero, triggers internal cleanup which detaches the SSE watcher,
 * closes pending file descriptors, and frees the stream structure and associated socket.
 * This function can be called multiple times safely; subsequent calls have no effect after the first.
 *
 * @param stream Stream to close
 *
 * @note Safe to call multiple times. Has no effect after first call.
 *       Triggers close/error callbacks unless already closed.
 *       Final cleanup happens when refcount reaches zero.
 */
extern void sse_stream_close(sse_stream_t *stream) ;

/**
 * @brief Modifies the I/O event monitoring for an SSE stream.
 *
 * Updates the internal SSE watcher to monitor for SSE_READ events (always) and SSE_WRITE events
 * (only when `io.want_flush` is true, indicating pending data to be flushed).
 * This function is called internally by I/O functions and the SSE event callback.
 *
 * @param[in] stream Pointer to the SSE stream to modify. If NULL or if the stream
 *                   is already closed, the function returns immediately.
 *
 * @note This function is typically called internally when the stream's state changes.
 */
extern void sse_stream_modify(sse_stream_t *stream) ;

// Data I/O operations
/**
 * @brief Read available data from the associated file descriptor into the internal buffer.
 *
 * Delegates to `io_rb_read()` which performs the actual non-blocking read operation
 * (using `recvmsg` by default) into the internal `io` ring buffer. Handles EAGAIN/EWOULDBLOCK internally.
 * On graceful EOF (recvmsg returns 0), sets errno = 0 and returns 0.
 *
 * @param stream Stream to read from
 *
 * @return Number of bytes received, 0 for transient/EOF, -1 on error
 * @retval >0  Successfully read and queued n bytes of data
 * @retval 0   Either no data available (EAGAIN) or EOF (errno == 0)
 * @retval -1  Fatal error occurred (inspect errno)
 *
 * @errno EBADF      if stream is NULL or already closed
 * @errno EOVERFLOW  if input buffer is full
 *
 * @note Caller must distinguish EOF from EAGAIN by checking errno == 0.
 *       Ancillary FDs are extracted automatically by io_rb.
 */
extern ssize_t sse_stream_read(sse_stream_t *stream);

/**
 * @brief Flush pending output data and file descriptors to the associated file descriptor.
 *
 * Delegates to `io_rb_flush()` which sends buffered data and queued file descriptors.
 * Calls `sse_stream_modify()` after the flush attempt to update event monitoring.
 *
 * @param stream Stream to flush
 *
 * @return Total bytes of data sent (>0), 0 if blocked, -1 on fatal error
 * @retval >0 Some data successfully sent
 * @retval 0  Would block – try again when writable
 * @retval -1 Permanent error (check errno)
 *
 * @errno EBADF if stream is NULL or closed
 */
extern ssize_t sse_stream_flush(sse_stream_t *stream) ;

/**
 * @brief Queue data for transmission.
 *
 * Delegates to `io_rb_put()` to copy data into the internal output ring buffer.
 * Attempts partial flush if insufficient space. Calls `sse_stream_modify()` after queuing
 * to potentially enable write events if `io.want_flush` becomes true.
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
 */
extern int sse_stream_put(sse_stream_t *stream, const void *data, size_t count) ;

/**
 * @brief Queue vectored data for transmission.
 *
 * Delegates to `io_rb_putv()` to write data from multiple buffers atomically.
 * Calls `sse_stream_modify()` after queuing.
 *
 * @param stream Stream to write to
 * @param iov    Array of iovec structures containing data to send
 * @param iovcnt Number of iovec entries (must be > 0)
 *
 * @return 0 on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if stream, iov invalid, closed, or iovcnt <= 0
 * @errno EAGAIN if insufficient buffer space after attempted flush
 * @errno EOVERFLOW if total write size exceeds buffer capacity
 */
extern int sse_stream_putv(sse_stream_t *stream, const struct iovec *iov, int iovcnt) ;

/**
 * @brief Read data from the internal input buffer.
 *
 * Delegates to `io_rb_get()` to extract exactly `count` bytes from the internal input ring buffer.
 * Fails immediately if less than `count` bytes are available.
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
 * @note Does NOT advance buffer — uses io_rb_get internally.
 */
extern ssize_t sse_stream_get(sse_stream_t *stream, void *dst, size_t count) ;

// File descriptor operations
/**
 * @brief Queue a file descriptor for sending.
 *
 * Delegates to `io_rb_put_fd()` to queue the file descriptor in the internal `fdout` buffer.
 * Will be transmitted during the next flush operation using SCM_RIGHTS ancillary data.
 * Calls `sse_stream_modify()` after queuing.
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
 * @brief Queue vectored data and file descriptors for transmission.
 *
 * Delegates to `io_rb_putv_fd()` to atomically queue both data from multiple buffers and file descriptors.
 * Calls `sse_stream_modify()` after queuing.
 *
 * @param stream Stream to write to
 * @param iov    Array of iovec structures containing data to send
 * @param iovcnt Number of iovec entries (must be > 0)
 * @param fds    Array of file descriptors to send (can be NULL if fdcnt == 0)
 * @param fdcnt  Number of file descriptors (must be >= 0)
 *
 * @return 0 on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if stream, iov invalid, closed, iovcnt <= 0, or fdcnt < 0
 * @errno EAGAIN if insufficient buffer space after attempted flush
 * @errno EOVERFLOW if total write size or FD count exceeds buffer capacity
 *
 * @note Either all data and FDs are queued or none (atomic operation).
 *       FDs are consumed on successful put – caller loses ownership.
 */
extern int sse_stream_putv_fd(sse_stream_t *stream, const struct iovec *iov, int iovcnt, const int *fds, int fdcnt);

/**
 * @brief Receive one queued file descriptor from the internal buffer.
 *
 * Delegates to `io_rb_get_fd()` to extract the first file descriptor from the internal `fdin` buffer.
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
 * @brief Close all pending inbound and outbound file descriptors in the internal buffers.
 *
 * Calls `io_rb_close_fds()` on both the internal `fdin` and `fdout` ring buffers.
 * Closes any FDs waiting to be sent or received. Useful during cleanup to avoid leaks.
 * Safe to call at any time.
 *
 * @param stream Stream whose FD queues to clear
 *
 * @note Does NOT affect currently held FDs outside buffers.
 *       Idempotent — safe to call repeatedly.
 */
extern void sse_stream_close_fds(sse_stream_t *stream) ;

// Status queries
/**
 * @brief Get number of bytes available to read in the internal input buffer.
 *
 * Delegates to `io_rb_pending_in()` for the internal `io` object.
 *
 * @param stream Stream to query
 *
 * @return Byte count, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_in(const sse_stream_t *stream) ;

/**
 * @brief Get number of bytes pending for write in the internal output buffer.
 *
 * Delegates to `io_rb_pending_out()` for the internal `io` object.
 *
 * @param stream Stream to query
 *
 * @return Byte count, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_out(const sse_stream_t *stream) ;

/**
 * @brief Get number of file descriptors waiting to be read in the internal fdin buffer.
 *
 * Delegates to `io_rb_pending_fdin()` for the internal `io` object.
 *
 * @param stream Stream to query
 *
 * @return Count of FDs, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_fdin(const sse_stream_t *stream) ;

/**
 * @brief Get number of file descriptors waiting to be sent in the internal fdout buffer.
 *
 * Delegates to `io_rb_pending_fdout()` for the internal `io` object.
 *
 * @param stream Stream to query
 *
 * @return Count of FDs, or 0 if stream is NULL
 */
extern size_t sse_stream_pending_fdout(const sse_stream_t *stream) ;

/**
 * @brief Check if stream is closed.
 *
 * Checks the internal `closed` flag. Includes both user-initiated close and error-induced closure.
 *
 * @param stream Stream to check
 *
 * @return true if closed, false otherwise
 *
 * @note Returns true even if close is pending due to refcounting.
 */
extern bool sse_stream_is_closed(const sse_stream_t *stream) ;

/**
 * @brief Get last error code set on the stream.
 *
 * Returns the internal `error` field. Valid only if stream is closed due to error.
 *
 * @param stream Stream to query
 *
 * @return Error code (e.g., EPIPE, ECONNRESET), or EINVAL if stream is NULL, or ECANCELED if closed via sse_stream_close().
 *
 * @note Returns ECANCELED if closed via sse_stream_close().
 */
extern int sse_stream_get_error(const sse_stream_t *stream) ;

// Convenience functions for Unix domain sockets
/**
 * @brief Create a Unix domain server socket.
 *
 * Uses `socketunix_create`, `socketunix_bind_reuse`, and `socketunix_listen` to
 * create, bind to the specified path, and listen on a non-blocking, CLOEXEC
 * socket. Uses a default backlog if the provided one is 0.
 *
 * Binding goes through `socketunix_bind_reuse`: it takes an exclusive lock on a
 * sidecar file `"<path>.lock"`, unlinks any stale socket node, then binds. A
 * restart over an orphan socket node therefore succeeds instead of failing with
 * `EADDRINUSE`, while a second live binder is rejected with `EBUSY` rather than
 * stealing the bound socket.
 *
 * @param path    Null-terminated path to bind
 * @param backlog Listen backlog (use 0 for default)
 * @param fdlock  Out: descriptor of the held bind lock, owned by the caller
 *
 * @return File descriptor on success, -1 on error
 * @retval -1 errno is set
 *
 * @errno EINVAL if path or fdlock is NULL
 * @errno EBUSY if another process already holds the bind lock
 *
 * @note On success `*fdlock` receives the bind lock fd, now owned by the caller
 *       for the lifetime of the server; on failure `*fdlock` is left untouched.
 *       The lock fd is `O_CLOEXEC`, so an exec always releases it — but a forked
 *       child that runs to `_exit` WITHOUT exec inherits it and keeps the
 *       `flock` held. The caller MUST therefore close `*fdlock` explicitly in
 *       any such child; otherwise the orphan keeps the lock and bricks a later
 *       rebind (`EBUSY`). A `"<path>.lock"` file is left beside the socket, and
 *       the socket node itself is not unlinked on shutdown — remove both
 *       manually if desired.
 */
extern int sse_streamux_create_server(const char *path, int backlog, int *fdlock) ;

/**
 * @brief Connect to a Unix domain server.
 *
 * Uses `socketunix_create` and `socketunix_connect` to establish a non-blocking,
 * CLOEXEC connection to the specified path.
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
 * Uses `socketunix_accept` to accept a connection and return a non-blocking, CLOEXEC client socket.
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