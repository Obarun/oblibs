/*
 * stream.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * Buffered streams over a file descriptor, the oblibs-native replacement for
 * skalibs' `buffer`. Both sides are thin layers on top of rbuffer (the ring
 * buffer) and the io_*v primitives, with no function pointers and no transport
 * indirection -- the fd is always the sink/source:
 *
 *   - ostream (write side): bytes accumulate in the ring buffer and are drained
 *     to the fd with io_allwritev.
 *   - istream (read side): bytes are pulled from the fd into the ring buffer in
 *     chunks and handed out on demand. istream_get is a blocking convenience;
 *     istream_fill + istream_getnofill are the building blocks for non-blocking,
 *     event-loop-driven reads (poll/iopause).
 */

#ifndef OB_STREAM_H
#define OB_STREAM_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <oblibs/attributes.h>
#include <oblibs/rbuffer.h>

/* ----------------------------------------------------------------------------
 * ostream -- write side
 * ------------------------------------------------------------------------- */

/** Default backing-storage size for the predefined ostream_1 / ostream_2. */
#define OSTREAM_OUTSIZE 8192

typedef struct ostream_s ostream ;
struct ostream_s
{
    rbuffer_t rb ; // ring buffer over caller-provided storage
    int fd ;       // sink file descriptor
} ;

/**
 * @brief Initialize an output stream over `fd` using caller-provided storage.
 *
 * @param o       The stream to initialize.
 * @param fd      The sink file descriptor.
 * @param storage Backing buffer (must outlive the stream), at least 2 bytes.
 * @param size    Size of `storage`. Effective capacity is `size - 1`.
 * @return 1 on success, 0 on failure (errno == EINVAL on bad arguments).
 */
extern int ostream_init(ostream *o, int fd, void *storage, size_t size) ;

/**
 * @brief Write all `len` bytes of `s` through the stream.
 *
 * Bytes are buffered; the buffer is flushed automatically to make room when
 * needed. A chunk larger than the buffer capacity is written straight to the
 * fd, bypassing the buffer. Either way the whole `len` is consumed on success.
 *
 * @return 1 if everything was buffered/written, 0 on I/O error (errno set).
 */
extern int ostream_put(ostream *o, char const *s, size_t len) ;

/**
 * @brief ostream_put() of a NUL-terminated string (excluding the NUL).
 * @return 1 on success, 0 on I/O error (errno set).
 */
extern int ostream_puts(ostream *o, char const *s) ;

/**
 * @brief ostream_put() each iovec in order.
 * @return 1 if all were written, 0 on the first I/O error (errno set).
 */
extern int ostream_putv(ostream *o, struct iovec const *iov, int iovcnt) ;

/**
 * @brief Drain the buffered bytes to the fd.
 * @return 1 if the buffer was fully drained, 0 on I/O error (errno set).
 */
extern int ostream_flush(ostream *o) ;

/**
 * @brief ostream_put() followed by ostream_flush().
 * @return 1 on success, 0 on I/O error (errno set).
 */
extern int ostream_putflush(ostream *o, char const *s, size_t len) ;

/**
 * @brief Write a NULL-terminated list of strings, in order (no flush).
 *
 * `ostream_cats(o, "a", "b", NULL)` writes `ab`. The argument list **must** end
 * with a `NULL` sentinel. Each string is written with ostream_put(); nothing is
 * flushed. This is the oblibs idiom for assembling output without a format
 * string: pre-format integers with types.h (`u32_fmt`, …) and pass the strings.
 *
 * @return 1 if all strings were written, 0 on the first I/O error (errno set).
 */
extern int ostream_cats(ostream *o, ...) attribute_sentinel ;

/**
 * @brief printf-style formatted write to the stream (no flush).
 *
 * The oblibs-native replacement for skalibs' `bprintf(buffer, fmt, ...)`. The
 * output is formatted with vsnprintf and written with ostream_put(); nothing is
 * flushed. Output up to an internal stack buffer is produced without heap use;
 * larger output is formatted via a single malloc/free (never a VLA).
 *
 * @return 1 if the formatted bytes were written, 0 on a formatting or I/O error
 *         (errno set on I/O error).
 */
extern int ostream_fmt(ostream *o, char const *fmt, ...) attribute_format(printf, 2, 3) ;

/** Predefined stream on fd 1 (stdout), with internal static storage. */
extern ostream ostream_1_ ;
#define ostream_1 (&ostream_1_)

/** Predefined stream on fd 2 (stderr), with internal static storage. */
extern ostream ostream_2_ ;
#define ostream_2 (&ostream_2_)

/* ----------------------------------------------------------------------------
 * istream -- read side
 * ------------------------------------------------------------------------- */

/** Default backing-storage size for the predefined istream_0. */
#define ISTREAM_INSIZE 8192

typedef struct istream_s istream ;
struct istream_s
{
    rbuffer_t rb ; // ring buffer over caller-provided storage
    int fd ;       // source file descriptor
} ;

/**
 * @brief Initialize an input stream over `fd` using caller-provided storage.
 *
 * @param o       The stream to initialize.
 * @param fd      The source file descriptor.
 * @param storage Backing buffer (must outlive the stream), at least 2 bytes.
 * @param size    Size of `storage`. Effective capacity is `size - 1`.
 * @return 1 on success, 0 on failure (errno == EINVAL on bad arguments).
 */
extern int istream_init(istream *o, int fd, void *storage, size_t size) ;

/*
 * Three layers, pick the one that fits the fd:
 *
 *   - istream_get      : blocking convenience. "Give me up to len bytes."
 *   - istream_fill +   : the building blocks. Drive them from your event loop
 *     istream_getnofill  for a non-blocking fd (poll/iopause): fill when the fd
 *                        signals readable, getnofill to hand buffered bytes out.
 *
 * istream_fill is the only function that touches the fd; istream_getnofill never
 * does; istream_get is fill+getnofill looped for you.
 */

/**
 * @brief Hand out up to `len` already-buffered bytes into `s`. Never reads the fd.
 *
 * The non-blocking-friendly getter: it only drains what istream_fill has already
 * pulled in, so it never blocks and never fails.
 *
 * @return Number of bytes copied (0 if the buffer is empty).
 */
extern size_t istream_getnofill(istream *o, char *s, size_t len) ;

/**
 * @brief Do exactly one read from the fd into the buffer.
 *
 * The only function that touches the fd. EINTR is retried internally. Call it
 * when (and only when) you need more bytes than istream_getnofill can give -- on
 * a non-blocking fd, typically when your event loop reports the fd readable.
 *
 * @return > 0 : number of bytes pulled into the buffer.
 *           0 : EOF (the peer closed; no more data will ever come).
 *          -1 : error, errno set. On a non-blocking fd with nothing available
 *               right now this is EAGAIN/EWOULDBLOCK (benign: wait and retry).
 *               ENOBUFS means the buffer is already full (drain it first).
 */
extern ssize_t istream_fill(istream *o) ;

/**
 * @brief Read up to `len` bytes into `s`, blocking until they arrive or EOF.
 *
 * The convenience layer for **blocking** fds: it loops istream_fill +
 * istream_getnofill until `len` bytes are delivered or the fd reaches EOF.
 * Bytes pulled beyond `len` stay buffered for the next call.
 *
 * @return Number of bytes read (0..len). A count `< len` means EOF was reached
 *         first. Returns -1 on a read error (errno set).
 *
 * @note Blocking-oriented. On a **non-blocking** fd a would-block surfaces as -1
 *       with errno EAGAIN/EWOULDBLOCK (and the call may have consumed some bytes):
 *       do not use istream_get there -- drive istream_fill + istream_getnofill
 *       from your event loop instead.
 */
extern ssize_t istream_get(istream *o, char *s, size_t len) ;

/** Predefined stream on fd 0 (stdin), with internal static storage. */
extern istream istream_0_ ;
#define istream_0 (&istream_0_)

#endif
