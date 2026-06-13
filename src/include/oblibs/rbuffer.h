/*
 * rbuffer.h
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
 * This header provides a simple, efficient ring buffer (circular buffer)
 * suitable for embedded and systems programming. It supports byte-level
 * reads/writes, scatter/gather I/O via iovec, and safe pointer manipulation.
 * Designed for SPSC (single-producer, single-consumer) use only.
 *
 * Buffer invariant:
 *   - Buffer is empty when read index == write index.
 *   - Buffer is full when (write + 1) wrapped == read index.
 *   - Effective capacity is (size - 1) to distinguish full/empty states.
 */

#ifndef RBUFFER_H
#define RBUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/uio.h>

/**
 * @struct rbuffer_s
 * @brief Ring buffer control structure.
 *
 * Manages a circular buffer using indices into a fixed-size byte array.
 * The buffer must be initialized with storage of at least 2 bytes.
 * Effective capacity is always (a - 1) to preserve empty/full distinction.
 *
 * @var b
 * Pointer to the caller-provided storage buffer.
 *
 * @var a
 * Total allocated size of the buffer (must be >= 2).
 *
 * @var r
 * Read index (points to next byte to read).
 *
 * @var w
 * Write index (points to next byte to write).
 */
struct rbuffer_s
{
    uint8_t *b ; // caller-provided storage of size 'a'
    size_t   a ; // total slots in buf (>= 2); effective capacity is (a - 1)
    size_t   r ; // read index
    size_t   w ; // write index
} ;
typedef struct rbuffer_s rbuffer_t ;

#define RBUFFER_ZERO { 0, 0, 0, 0 }

// Helper macros
#define RB_MIN(a, b) ((a) < (b) ? (a) : (b))

static inline int rbuffer_init(rbuffer_t *restrict b, void *restrict storage, size_t size)
{
    if (b == NULL || storage == NULL || size < 2)
    return (errno = EINVAL, 0) ;
    b->b = (uint8_t *)storage ;
    b->a = size ;
    b->r = 0 ;
    b->w = 0 ;
    return 1 ;
}

/**
 * @brief Wrap an index within the buffer bounds.
 *
 * Efficiently wraps an index using conditional subtraction instead of modulo.
 * Safe only if idx < 2 * size.
 *
 * @param idx The index to wrap.
 * @param size The size of the buffer.
 * @return The wrapped index (0 <= result < size).
 */
static inline size_t rbuffer_wrap(size_t idx, size_t size)
{
    return idx >= size ? idx - size : idx ;
}

/**
 * @brief Get the effective capacity of the ring buffer.
 *
 * Due to the need to distinguish full/empty states, one slot is reserved.
 *
 * @param b Pointer to the ring buffer.
 * @return Effective capacity = b->a - 1.
 */
static inline size_t rbuffer_capacity(const rbuffer_t *b)
{
    return b->a - 1 ;
}

/**
 * @brief Get the number of bytes currently stored in the buffer.
 *
 * @param b Pointer to the ring buffer.
 * @return Number of readable bytes (0 <= result <= capacity).
 */
static inline size_t rbuffer_used(const rbuffer_t *b)
{
    return b->w >= b->r ? b->w - b->r : b->a - b->r + b->w ;
}

/**
 * @brief Get the number of free bytes available for writing.
 *
 * @param b Pointer to the ring buffer.
 * @return Number of writable bytes (0 <= result <= capacity).
 */
static inline size_t rbuffer_available(const rbuffer_t *b)
{
    return rbuffer_capacity(b) - rbuffer_used(b) ;
}

/**
 * @brief Check if the ring buffer is empty.
 *
 * @param b Pointer to the ring buffer.
 * @return true if no data is available to read, false otherwise.
 */
static inline bool rbuffer_isempty(const rbuffer_t *b)
{
    return b->r == b->w ;
}

/**
 * @brief Check if the ring buffer is full.
 *
 * One slot is always reserved to distinguish full from empty.
 *
 * @param b Pointer to the ring buffer.
 * @return true if no space is available for writing, false otherwise.
 */
static inline bool rbuffer_isfull(const rbuffer_t *b)
{
    return rbuffer_wrap(b->w + 1, b->a) == b->r ;
}

// write

/**
 * @brief Write bytes into the ring buffer.
 *
 * Copies up to `n` bytes from `data` into the buffer. Stops if buffer becomes full.
 *
 * @param b Pointer to the ring buffer.
 * @param data Pointer to source data.
 * @param n Maximum number of bytes to write.
 *
 * @return Number of bytes actually written (0 <= result <= min(n, available)).
 *
 * @note Safe to call even if buffer is full (returns 0).
 */
static inline size_t rbuffer_write(rbuffer_t *restrict b, const void *restrict data, size_t n)
{
    const uint8_t *src = (const uint8_t *)data ;
    size_t space = rbuffer_available(b) ;
    size_t to_write = RB_MIN(n, space) ;
    size_t w = b->w ;

    if (to_write == 0)
    return 0 ;

    size_t chunck = RB_MIN(to_write, b->a - w) ;
    for (size_t i = 0 ; i < chunck ; i++) {
        b->b[w + i] = src[i];
    }

    size_t remain = to_write - chunck ;
    for (size_t i = 0 ; i < remain ; i++) {
        b->b[i] = src[chunck + i] ;
    }

    b->w = rbuffer_wrap(b->w + to_write, b->a) ;

    return to_write ;
}

/**
 * @brief Write data from an array of iovec structures.
 *
 * Writes sequentially from each iovec until buffer is full or all iovecs are consumed.
 *
 * @param b Pointer to the ring buffer.
 * @param v Array of iovec structures.
 * @param vcount Number of iovec entries.
 *
 * @return Total number of bytes written.
 *
 * @note Stops early if buffer becomes full during a write.
 */
static inline size_t rbuffer_writev(rbuffer_t *restrict b, const struct iovec *v, int vcount)
{
    size_t total = 0 ;

    for (int i = 0 ; i < vcount && !rbuffer_isfull(b) ; i++) {

        size_t written = rbuffer_write(b, v[i].iov_base, v[i].iov_len) ;
        total += written ;

        if (written < v[i].iov_len)
            break ;
    }

    return total ;
}

/**
 * @brief Write a NUL-terminated string (excluding the NUL byte).
 *
 * @param b Pointer to the ring buffer.
 * @param s Pointer to the string to write.
 *
 * @return Number of bytes written (strlen(s)).
 *
 * @note Does NOT write the terminating '\0'.
 */
static inline size_t rbuffer_writes(rbuffer_t *restrict b, const char *restrict s)
{
    size_t len = 0 ;
    while (s[len]) len++ ; // strlen inline
    return rbuffer_write(b, s, len) ;
}

/**
 * @brief Fill iovec array with writable buffer regions.
 *
 * Describes up to two contiguous writable memory regions in the ring buffer.
 * Useful for zero-copy writes (e.g., read() or recvmsg() directly into buffer).
 *
 * @param b Pointer to the ring buffer.
 * @param vec Array of 2 iovec structures to fill.
 *
 * @return Number of valid iovec entries (0, 1, or 2).
 *
 * @note vec must point to array of at least 2 struct iovec.
 * @note If return is 0, buffer is full.
 */
static inline unsigned rbuffer_wiovec(const rbuffer_t *b, struct iovec vec[2])
{
    size_t space = rbuffer_available(b) ;
    if (!space)
    return 0 ;

    size_t w = b->w ;
    size_t r = b->r ;
    size_t size = b->a ;

    if (w < r) {
        /* Writable region: [w .. r-1] */
        vec[0].iov_base = b->b + w ;
        vec[0].iov_len = r - w - 1 ;
        return 1 ;
    }

    /* Two potential regions: [w .. size] and [0 .. r-1] */
    size_t alen = size - w - (r == 0 ? 1 : 0) ;  // If r == 0, reserve last slot
    size_t blen = (r == 0) ? 0 : r - 1 ; // [0 ... r-1], so if r == 0, no second part

    unsigned count = 0 ;

    if (alen > 0) {
        vec[count].iov_base = b->b + w ;
        vec[count].iov_len = alen ;
        count++ ;
    }

    if (blen > 0) {
        vec[count].iov_base = b->b ;
        vec[count].iov_len = blen ;
        count++ ;
    }

    return count ; // count == 0 -> buffer full
}

/**
 * @brief Advance write pointer by n bytes (unsafe).
 *
 * Directly moves the write pointer forward. Does not validate available space.
 *
 * @param b Pointer to the ring buffer.
 * @param n Number of bytes to advance.
 *
 * @pre n <= rbuffer_available(b)
 *
 * @warning Unsafe — caller must ensure sufficient space.
 */
static inline void rbuffer_wadvance(rbuffer_t *restrict b, size_t n)
{
    b->w = rbuffer_wrap(b->w + n, b->a) ;
}

/**
 * @brief Reserve space by advancing the write pointer.
 *
 * Advances the write pointer by up to `n` bytes, limited by available space.
 *
 * @param b Pointer to the ring buffer.
 * @param n Requested number of bytes to reserve.
 *
 * @return Actual number of bytes reserved (0 <= result <= min(n, available)).
 */
static inline size_t rbuffer_wreserve(rbuffer_t *restrict b, size_t n)
{
    size_t space = rbuffer_available(b) ;
    size_t reserved = RB_MIN(n, space) ;
    rbuffer_wadvance(b, reserved) ;
    return reserved ;
}

/**
 * @brief Rewind write pointer by n bytes (unsafe).
 *
 * Moves the write pointer backward, effectively "un-writing" data.
 *
 * @param b Pointer to the ring buffer.
 * @param n Number of bytes to rewind.
 *
 * @pre n <= rbuffer_used(b)
 *
 * @warning Unsafe — caller must ensure not to rewind past read pointer.
 */
static inline void rbuffer_wrewind(rbuffer_t *restrict b, size_t n)
{
    b->w = b->w >= n ? b->w - n : b->a - (n - b->w) ;
}

/**
 * @brief Safely rewind write pointer by up to n bytes.
 *
 * Rewinds the write pointer without exceeding the amount of data written.
 *
 * @param b Pointer to the ring buffer.
 * @param n Maximum number of bytes to rewind.
 *
 * @return Actual number of bytes rewound (0 <= result <= min(n, used)).
 */
static inline size_t rbuffer_wrewind_safe(rbuffer_t *restrict b, size_t n)
{
    size_t len = rbuffer_used(b) ;
    size_t rewound = RB_MIN(n, len) ;
    rbuffer_wrewind(b, rewound) ;
    return rewound ;
}

// read


/**
 * @brief Read bytes from the ring buffer.
 *
 * Copies up to `n` bytes from buffer into `buf`. Advances read pointer.
 *
 * @param b Pointer to the ring buffer.
 * @param buf Destination buffer.
 * @param n Maximum number of bytes to read.
 *
 * @return Number of bytes actually read (0 <= result <= min(n, used)).
 *
 * @note Safe to call even if buffer is empty (returns 0).
 */
static inline size_t rbuffer_read(rbuffer_t *restrict b, void *restrict buf, size_t n)
{
    uint8_t *dst = (uint8_t *)buf ;
    size_t available = rbuffer_used(b) ;
    size_t to_read = RB_MIN(n, available) ;
    size_t r = b->r ;

    if (to_read == 0)
        return 0 ;

    size_t chunck = RB_MIN(to_read, b->a - r) ;
    for (size_t i = 0; i < chunck; i++) {
        dst[i] = b->b[r + i] ;
    }

    size_t remaining = to_read - chunck ;
    for (size_t i = 0; i < remaining; i++) {
        dst[chunck + i] = b->b[i] ;
    }

    b->r = rbuffer_wrap(b->r + to_read, b->a) ;
    return to_read ;
}

/**
 * @brief Read bytes without advancing the read pointer.
 *
 * Copies up to `n` bytes from buffer into `buf`. Does NOT consume:
 * the read pointer is left untouched so the same data can be read again.
 *
 * @param b Pointer to the ring buffer.
 * @param buf Destination buffer.
 * @param n Maximum number of bytes to peek.
 *
 * @return Number of bytes copied (0 <= result <= min(n, used)).
 *
 * @note Safe to call even if buffer is empty (returns 0).
 */
static inline size_t rbuffer_peek(const rbuffer_t *restrict b, void *restrict buf, size_t n)
{
    uint8_t *dst = (uint8_t *)buf ;
    size_t available = rbuffer_used(b) ;
    size_t to_read = RB_MIN(n, available) ;
    size_t r = b->r ;

    if (to_read == 0)
        return 0 ;

    size_t chunck = RB_MIN(to_read, b->a - r) ;
    for (size_t i = 0; i < chunck; i++) {
        dst[i] = b->b[r + i] ;
    }

    size_t remaining = to_read - chunck ;
    for (size_t i = 0; i < remaining; i++) {
        dst[chunck + i] = b->b[i] ;
    }

    return to_read ;
}

/**
 * @brief Scatter read into an array of iovec structures.
 *
 * Reads sequentially into each iovec until buffer is empty or all iovecs filled.
 *
 * @param b Pointer to the ring buffer.
 * @param v Array of iovec structures.
 * @param vcount Number of iovec entries.
 *
 * @return Total number of bytes read.
 *
 * @note Stops early if buffer becomes empty during a read.
 */

static inline size_t rbuffer_readv(rbuffer_t *restrict b, const struct iovec *v, int vcount)
{
    size_t total = 0 ;

    for (int i = 0 ; i < vcount && !rbuffer_isempty(b) ; i++) {

        size_t read_bytes = rbuffer_read(b, v[i].iov_base, v[i].iov_len) ;
        total += read_bytes ;

        if (read_bytes < v[i].iov_len)
        break ;
    }

    return total;
}

/**
 * @brief Fill iovec array with readable buffer regions.
 *
 * Describes up to two contiguous readable memory regions in the ring buffer.
 * Useful for zero-copy reads (e.g., write() or sendmsg() directly from buffer).
 *
 * @param b Pointer to the ring buffer.
 * @param vec Array of 2 iovec structures to fill.
 *
 * @return Number of valid iovec entries (0, 1, or 2).
 *
 * @note vec must point to array of at least 2 struct iovec.
 * @note If return is 0, buffer is empty.
 */
static inline unsigned rbuffer_riovec(const rbuffer_t *b, struct iovec vec[2])
{
    size_t len = rbuffer_used(b) ;
    if (len == 0)
        return 0 ;

    size_t r = b->r ;
    size_t w = b->w ;
    size_t size = b->a ;

    if (r < w) {
        /* Single region: [r .. w] */
        vec[0].iov_base = b->b + r ;
        vec[0].iov_len = w - r ;
        return 1 ;
    }

    /* Two regions: [r .. size] and [0 .. w] */
    vec[0].iov_base = b->b + r ;
    vec[0].iov_len = size - r ;

    if (w > 0) {
        vec[1].iov_base = b->b ;
        vec[1].iov_len = w ;
        return 2 ;
    }

    return 1 ;
}

/**
 * @brief Advance read pointer by n bytes (unsafe).
 *
 * Directly moves the read pointer forward. Does not validate available data.
 *
 * @param b Pointer to the ring buffer.
 * @param n Number of bytes to advance.
 *
 * @pre n <= rbuffer_used(b)
 *
 * @warning Unsafe — caller must ensure sufficient data.
 */
static inline void rbuffer_radvance(rbuffer_t *restrict b, size_t n)
{
    b->r = rbuffer_wrap(b->r + n, b->a) ;
}

/**
 * @brief Consume (skip) bytes by advancing the read pointer.
 *
 * Advances the read pointer by up to `n` bytes, limited by available data.
 *
 * @param b Pointer to the ring buffer.
 * @param n Requested number of bytes to skip.
 *
 * @return Actual number of bytes consumed (0 <= result <= min(n, used)).
 */
static inline size_t rbuffer_rskip(rbuffer_t *restrict b, size_t n)
{
    size_t available = rbuffer_used(b) ;
    size_t consumed = RB_MIN(n, available) ;
    rbuffer_radvance(b, consumed) ;
    return consumed ;
}

/**
 * @brief Rewind read pointer by n bytes (unsafe).
 *
 * Moves the read pointer backward, effectively "un-reading" data.
 *
 * @param b Pointer to the ring buffer.
 * @param n Number of bytes to rewind.
 *
 * @pre n <= rbuffer_available(b) (i.e., space behind read pointer)
 *
 * @warning Unsafe — caller must ensure not to rewind into unwritten area.
 */
static inline void rbuffer_rrewind(rbuffer_t *restrict b, size_t n)
{
    b->r = b->r >= n ? b->r - n : b->a - (n - b->r) ;
}

/**
 * @brief Safely rewind read pointer by up to n bytes.
 *
 * Rewinds the read pointer without exceeding available unwritten space.
 *
 * @param b Pointer to the ring buffer.
 * @param n Maximum number of bytes to rewind.
 *
 * @return Actual number of bytes rewound (0 <= result <= min(n, available behind read pointer)).
 */
static inline size_t rbuffer_rrewind_safe(rbuffer_t *restrict b, size_t n)
{
    // Calculate how much we can actually rewind without going past the start of valid data
    size_t max_rewind = rbuffer_capacity(b) - rbuffer_used(b) ;  // How much space is behind read pointer
    size_t rewound = RB_MIN(n, max_rewind) ;
    rbuffer_rrewind(b, rewound) ;
    return rewound ;
}

#endif