/*
 * io_rb.h
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
 * @brief Ring-buffer-based I/O abstraction with support for vectored I/O and file descriptor passing.
 *
 * This module provides a buffered I/O layer built on top of ring buffers (`rbuffer_t`)
 * for both input and output directions. It supports custom transport operations
 * (e.g., for sockets, TLS, or custom protocols) and handles passing file descriptors
 * over Unix domain sockets using `sendmsg`/`recvmsg` and `SCM_RIGHTS` ancillary data.
 *
 * The API is designed to be non-blocking friendly and integrates cleanly with event loops.
 * It uses separate ring buffers for regular data and file descriptors for each direction.
 *
 */

#ifndef OB_IO_RB_H
#define OB_IO_RB_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <oblibs/rbuffer.h>

#define IO_RB_BUFFER_SIZE 8192
#define IO_RB_MAX_FD 253 // SCM_MAX_FD on Linux

typedef struct io_rb_s io_rb_t ;

/**
 * @brief Input or output buffer using a ring buffer and static storage.
 *
 * Each `io_rb_buffer_t` holds a ring buffer (`rbuffer_t`) and a fixed-size
 * internal data array used as backing storage.
 */
struct io_rb_buffer_s
{
    rbuffer_t buf ;
    uint8_t data[IO_RB_BUFFER_SIZE] ;
} ;
typedef struct io_rb_buffer_s io_rb_buffer_t ;

/**
 * @brief Structure representing scatter-gather vectors and associated file descriptors.
 *
 * This structure groups an array of `iovec` for data and an array of file descriptors,
 * potentially used for constructing messages for transmission.
 * (Note: This structure is defined in the header but may not be directly used by all public functions).
 */
struct io_rb_iovec_s
{
    struct iovec *iov ; // Array of iovec structures describing message parts (e.g., header + payload).
    int niov ;          // Number of elements in iov (must be >= 1).
    int *afd ;          // Array of associated file descriptors (may be NULL).
    int nfd ;           // Number of file descriptors in afd (may be 0).
} ;
typedef struct io_rb_iovec_s io_rb_iovec_t ;

/**
 * @brief Transport-specific low-level I/O operations.
 *
 * These callbacks allow the I/O ring buffer layer to delegate actual system
 * calls or protocol-specific logic (e.g., TLS read/write) to the transport layer.
 * If callbacks are not provided, default `recvmsg`/`sendmsg` operations are used.
 */
struct io_rb_ops_s
{
    /**
     * @brief Perform a raw read into scatter-gather buffers.
     *
     * This function should read data from the underlying transport into the buffers
     * described by `iov`. It should handle the actual system call or protocol-specific
     * read operation.
     *
     * @param[in]  io      I/O ring buffer object.
     * @param[out] iov     Array of iovec structures describing the buffers to fill.
     * @param[in]  iovcnt  Number of elements in `iov`.
     * @param[in]  data    User-provided context data.
     * @return Number of bytes read on success, -1 on error (set errno). Return 0 for EOF or non-blocking would-block.
     */
    ssize_t (*read)(io_rb_t *io, const struct iovec *iov, int iovcnt, void *data) ;

    /**
     * @brief Perform a raw write from scatter-gather buffers.
     *
     * This function should write data from the buffers described by `iov` to the
     * underlying transport. It should handle the actual system call or protocol-specific
     * write operation.
     *
     * @param[in] io      I/O ring buffer object.
     * @param[in] iov     Array of iovec structures containing the data to write.
     * @param[in] iovcnt  Number of elements in `iov`.
     * @param[in] data    User-provided context data.
     * @return Number of bytes written on success, -1 on error (set errno). Return 0 for non-blocking would-block.
     */
    ssize_t (*write)(io_rb_t *io, const struct iovec *iov, int iovcnt, void *data) ;
} ;
typedef struct io_rb_ops_s io_rb_ops_t ;

/**
 * @brief Main I/O state structure managing input and output buffers.
 *
 * This structure manages separate ring buffers for regular data and file descriptors
 * in both input and output directions. It holds the associated file descriptor
 * and optional transport-specific operations.
 */
struct io_rb_s
{
    io_rb_buffer_t in, out ;     // Input/Output buffer for regular data
    io_rb_buffer_t fdin, fdout ; // Input/Output buffer for file descriptor passing
    int fd ;                     // Associated file descriptor of the transport

    const io_rb_ops_t *ops ;     // Transport-specific operations (can be NULL for default socket ops)

    bool want_flush ;            // True if output buffer needs flushing
    int refcount ;               // Reference count for lifetime management
} ;

/**
 * @brief Compute total length of an array of iovec structures.
 *
 * Calculates the sum of all `iov_len` fields in the provided array of `iovec`.
 *
 * @param[in] iov     Array of iovec structures.
 * @param[in] iovcnt  Number of elements in the array.
 * @return Total byte count across all `iov_len` fields.
 */
extern size_t io_rb_iovec_len(const struct iovec *iov, int iovcnt) ;

/**
 * @brief Initialize an I/O ring buffer instance.
 *
 * Initializes the internal ring buffers for data and file descriptors,
 * sets the associated file descriptor, and configures transport operations.
 * Initializes the reference count to 1.
 *
 * @param[out] io   I/O ring buffer structure to initialize.
 * @param[in]  ops  Pointer to transport-specific operations (can be NULL for default socket operations).
 * @param[in]  fd   File descriptor associated with the transport.
 * @return 1 on success, 0 on failure (errno set).
 */
extern int io_rb_init(io_rb_t *io, const io_rb_ops_t *ops, int fd) ;

/**
 * @brief Read data from the associated file descriptor into the input buffer.
 *
 * Attempts to read data from `io->fd` into available space in the `in` ring buffer.
 * If custom `ops->read` is provided, it is used; otherwise, `recvmsg` is used.
 * Handles receiving file descriptors via `SCM_RIGHTS` ancillary data if applicable.
 * This function is non-blocking: it returns 0 if no data is available without blocking.
 *
 * @param[in,out] io   I/O ring buffer object.
 * @param[in]     data User-provided context data passed to custom read operation.
 * @return
 *   - >0: Number of bytes read and added to the input buffer.
 *   - 0:  No data available (would block) or EOF reached (check `errno`).
 *   - -1: Error occurred (errno set).
 */
extern ssize_t io_rb_read(io_rb_t *io, void *data) ;

/**
 * @brief Flush pending output data and file descriptors to the associated file descriptor.
 *
 * Attempts to write data from the `out` ring buffer and file descriptors from the `fdout`
 * ring buffer to `io->fd`. If custom `ops->write` is provided, it is used; otherwise,
 * `sendmsg` is used, potentially including `SCM_RIGHTS` ancillary data. Writes as much
 * as possible until the buffer is empty or a non-blocking write would block.
 *
 * The `want_flush` flag is cleared if the data buffer (`out`) becomes empty after flushing.
 * The flag remains set if the file descriptor buffer (`fdout`) still has data.
 *
 * @param[in,out] io   I/O ring buffer object.
 * @param[in]     data User-provided context data passed to custom write operation.
 * @return
 *   - >=0: Total number of data bytes successfully flushed.
 *   - -1:  Error occurred (errno set).
 */
extern ssize_t io_rb_flush(io_rb_t *io, void *data) ;

/**
 * @brief Queue data for transmission by copying into the output buffer.
 *
 * Attempts to copy `count` bytes from `data` into the `out` ring buffer.
 * If the output buffer lacks sufficient space, it first attempts to flush
 * existing data by calling `io_rb_flush`. Sets the `want_flush` flag to true
 * upon successful addition to the output buffer.
 *
 * @param[in,out] io    I/O ring buffer object.
 * @param[in]     data  Pointer to the data to be queued.
 * @param[in]     count Number of bytes to queue.
 * @param[in]     ctx   User-provided context data passed to `io_rb_flush` if needed.
 * @return 0 on success, -1 on error (errno set to EINVAL, EAGAIN if flush fails, or EOVERFLOW if buffer is full).
 */
extern int io_rb_put(io_rb_t *io, const void *data, size_t count, void *ctx) ;

/**
 * @brief Queue vectored data for transmission by copying into the output buffer.
 *
 * Attempts to copy data described by an array of `iovec` structures into the `out` ring buffer.
 * If the output buffer lacks sufficient space, it first attempts to flush existing data.
 * Sets the `want_flush` flag to true upon successful addition.
 *
 * @param[in,out] io      I/O ring buffer object.
 * @param[in]     iov     Array of iovec structures describing the data to queue.
 * @param[in]     iovcnt  Number of iovec structures in the array (must be > 0).
 * @param[in]     ctx     User-provided context data passed to `io_rb_flush` if needed.
 * @return 0 on success, -1 on error (errno set).
 */
extern int io_rb_putv(io_rb_t *io, const struct iovec *iov, int iovcnt, void *ctx) ;

/**
 * @brief Queue a file descriptor for transmission.
 *
 * Attempts to queue a single file descriptor into the `fdout` ring buffer for later transmission
 * via `io_rb_flush`. Sets the `want_flush` flag to true upon successful addition.
 *
 * @param[in,out] io I/O ring buffer object.
 * @param[in]     fd The file descriptor to queue for sending.
 * @return 0 on success, -1 on error (errno set, typically EOVERFLOW if fd buffer is full or EINVAL).
 */
extern int io_rb_put_fd(io_rb_t *io, int fd) ;

/**
 * @brief Queue vectored data and file descriptors for transmission.
 *
 * Attempts to copy data described by `iov` and file descriptors from `fds` into
 * their respective output buffers (`out` and `fdout`). Performs atomic queuing:
 * if queuing file descriptors fails after data was queued, the data write is rolled back.
 * Sets the `want_flush` flag to true if any data or file descriptors were added.
 *
 * @param[in,out] io      I/O ring buffer object.
 * @param[in]     iov     Array of iovec structures describing the data to queue.
 * @param[in]     iovcnt  Number of iovec structures in the array (must be > 0).
 * @param[in]     fds     Array of file descriptors to queue (can be NULL if `nfd` is 0).
 * @param[in]     nfd     Number of file descriptors in the `fds` array (must be >= 0).
 * @param[in]     ctx     User-provided context data passed to `io_rb_flush` if needed.
 * @return 0 on success, -1 on error (errno set).
 */
extern int io_rb_putv_fd(io_rb_t *io, const struct iovec *iov, int iovcnt, const int *fds, int nfd, void *ctx) ;

/**
 * @brief Retrieve data from the input buffer.
 *
 * Copies `count` bytes from the `in` ring buffer into the destination buffer `dst`.
 * Does not perform any I/O operations; only consumes already-buffered data.
 *
 * @param[in,out] io    I/O ring buffer object.
 * @param[out]    dst   Destination buffer to copy data into.
 * @param[in]     count Number of bytes to retrieve.
 * @return
 *   - >0: Number of bytes copied (equals `count` on success).
 *   - -1: Not enough data available (errno = EAGAIN) or invalid arguments (errno = EINVAL).
 */
extern ssize_t io_rb_get(io_rb_t *io, void *dst, size_t count) ;

/**
 * @brief Retrieve a file descriptor from the input buffer.
 *
 * Copies a single file descriptor from the `fdin` ring buffer into the integer pointed to by `fd`.
 * Does not perform any I/O operations; only consumes already-buffered file descriptors.
 *
 * @param[in,out] io I/O ring buffer object.
 * @param[out]    fd Pointer to an integer where the retrieved file descriptor will be stored.
 * @return
 *   - >0: Number of bytes copied (equals `sizeof(int)` on success).
 *   - -1: Not enough data available (errno = EAGAIN) or invalid arguments (errno = EINVAL).
 */
extern ssize_t io_rb_get_fd(io_rb_t *io, int *fd) ;

/**
 * @brief Get the number of bytes pending in the input data buffer.
 *
 * Returns the number of bytes available for reading in the `in` ring buffer.
 *
 * @param[in] io I/O ring buffer object (can be NULL).
 * @return Number of bytes available, or 0 if `io` is NULL.
 */
extern size_t io_rb_pending_in(const io_rb_t *io) ;

/**
 * @brief Get the number of bytes pending in the output data buffer.
 *
 * Returns the number of bytes queued for writing in the `out` ring buffer.
 *
 * @param[in] io I/O ring buffer object (can be NULL).
 * @return Number of bytes queued, or 0 if `io` is NULL.
 */
extern size_t io_rb_pending_out(const io_rb_t *io) ;

/**
 * @brief Get the number of bytes pending in the input file descriptor buffer.
 *
 * Returns the number of bytes (representing file descriptors) available for reading in the `fdin` ring buffer.
 *
 * @param[in] io I/O ring buffer object (can be NULL).
 * @return Number of bytes available, or 0 if `io` is NULL.
 */
extern size_t io_rb_pending_fdin(const io_rb_t *io) ;

/**
 * @brief Get the number of bytes pending in the output file descriptor buffer.
 *
 * Returns the number of bytes (representing file descriptors) queued for writing in the `fdout` ring buffer.
 *
 * @param[in] io I/O ring buffer object (can be NULL).
 * @return Number of bytes queued, or 0 if `io` is NULL.
 */
extern size_t io_rb_pending_fdout(const io_rb_t *io) ;

/**
 * @brief Rewind the input data buffer read pointer.
 *
 * Moves the read position of the `in` ring buffer backward by `count` bytes.
 * This allows previously read data to be read again. Intended for use cases like lookahead parsing.
 *
 * @param[in,out] io    I/O ring buffer object.
 * @param[in]     count Number of bytes to rewind the read pointer.
 */
extern void io_rb_restore(io_rb_t *io, size_t count) ;

/**
 * @brief Reserve space in the output data buffer for direct writing.
 *
 * Attempts to reserve `count` bytes in the `out` ring buffer for direct writing.
 * Returns the number of contiguous bytes available at the current write position.
 * Sets the `want_flush` flag to true if any space is successfully reserved.
 *
 * @param[in,out] io    I/O ring buffer object.
 * @param[in]     count Desired number of bytes to reserve.
 * @return Number of bytes actually reserved (may be less than `count` or 0 if no space is available), or -1 on error (errno set).
 */
extern ssize_t io_rb_reserve_output(io_rb_t *io, size_t count) ;

/**
 * @brief Increment the reference count of an I/O ring buffer object.
 *
 * Increments the internal reference counter. This is used for managing the object's lifetime
 * in scenarios where multiple entities might hold a reference.
 *
 * @param[in,out] io I/O ring buffer object.
 */
extern void io_rb_ref(io_rb_t *io) ;

/**
 * @brief Decrement the reference count of an I/O ring buffer object.
 *
 * Decrements the internal reference counter. This is used for managing the object's lifetime.
 * When the reference count reaches zero, the caller is responsible for freeing the object
 * if it owns it.
 *
 * @param[in,out] io I/O ring buffer object.
 */
extern void io_rb_unref(io_rb_t *io) ;

/**
 * @brief Prepare ancillary data message for sending file descriptors.
 *
 * Reads file descriptors from the `fdout` ring buffer and formats them into the provided
 * `cmsg` buffer in the format required by `sendmsg` for `SCM_RIGHTS`. The number of
 * file descriptors included is limited by the available space in `cmsg` and `IO_RB_MAX_FD`.
 * If successful, the read file descriptors are consumed from the `fdout` buffer.
 *
 * @param[in,out] io        I/O ring buffer object.
 * @param[out]    cmsg      Buffer to store the formatted ancillary data message.
 * @param[in]     cmsgsize  Size of the `cmsg` buffer.
 * @param[out]    cmsglen   Pointer to an integer where the length of the formatted message will be stored.
 * @return Number of file descriptors encoded on success, -1 on error (e.g., buffer too small).
 */
extern int io_rb_encode_cmsg(io_rb_t *io, char *cmsg, size_t cmsgsize, int *cmsglen) ;

/**
 * @brief Process ancillary data message received from `recvmsg`.
 *
 * Parses the `struct msghdr` returned by `recvmsg` to extract file descriptors
 * passed via `SCM_RIGHTS`. Successfully extracted file descriptors are stored
 * in the `fdin` ring buffer. If the `fdin` buffer overflows or the message is
 * malformed, received file descriptors are closed without being stored.
 *
 * @param[in,out] io   I/O ring buffer object.
 * @param[in]     msg  Pointer to the `struct msghdr` filled by `recvmsg`.
 * @return 0 on success, -1 on failure (e.g., buffer overflow, errno set to EOVERFLOW).
 */
extern int io_rb_decode_cmsg(io_rb_t *io, struct msghdr *msg) ;

/**
 * @brief Close all file descriptors remaining in a ring buffer.
 *
 * Reads file descriptors from the specified `rbuffer_t` and calls `close()` on each one.
 * Intended for cleanup to prevent resource leaks.
 *
 * @param[in,out] buf Ring buffer containing file descriptors to close.
 */
extern void io_rb_close_fds(rbuffer_t *buf) ;

#endif