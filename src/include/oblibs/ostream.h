/*
 * ostream.h
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
 * Buffered output stream over a file descriptor. A thin write-only layer on
 * top of rbuffer (the ring buffer) and the io_*v primitives: bytes accumulate
 * in the ring buffer and are drained to the fd with io_allwritev. No function
 * pointers, no transport indirection -- the sink is always the fd.
 *
 * This is the oblibs-native, write-side replacement for skalibs' `buffer`.
 * The symmetric read side (`istream`) will be added when a consumer needs it.
 */

#ifndef OB_OSTREAM_H
#define OB_OSTREAM_H

#include <stddef.h>
#include <sys/uio.h>

#include <oblibs/rbuffer.h>

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

/** Predefined stream on fd 1 (stdout), with internal static storage. */
extern ostream ostream_1_ ;
#define ostream_1 (&ostream_1_)

/** Predefined stream on fd 2 (stderr), with internal static storage. */
extern ostream ostream_2_ ;
#define ostream_2 (&ostream_2_)

#endif
