/*
 * io.h
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_IO_H
#define OB_IO_H

#include <stddef.h>
#include <sys/types.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0x40000000
#endif

/**
 * @brief Checks if an error code represents a non-blocking I/O retry condition.
 *
 * This macro evaluates whether the given error code corresponds to `EAGAIN`
 * or `EWOULDBLOCK`, which both indicate that a non-blocking read or write
 * operation would block and should be retried later.
 *
 * @param e The error code to check (typically `errno`).
 * @return Non-zero (true) if the error is EAGAIN or EWOULDBLOCK, 0 otherwise.
 */
#define error_isagain(e) (((e) == EAGAIN) || ((e) == EWOULDBLOCK))

extern int io_coe(int fd) ;
extern int io_uncoe(int fd) ;
extern int io_setfl(int fd, int flags) ;
extern int io_unsetfl(int fd, int flags) ;
extern int io_set_block(int fd) ;
extern int io_set_nonblock(int fd) ;
extern ssize_t io_open(const char *path, unsigned int flags) ;
extern ssize_t io_open_mode(const char *path, unsigned int flags, mode_t mode) ;
extern ssize_t io_read(int fd, char *store, size_t len) ;

/**
 * @brief Normalizes the result of a non-blocking read operation.
 *
 * This function processes the return value of a read-like system call to
 * provide consistent semantics for non-blocking I/O:
 *
 * - If the read failed with `EAGAIN` or `EWOULDBLOCK`, returns 0 and clears `errno`.
 * - If the read returned 0 (EOF), sets `errno` to `EPIPE` and returns -1.
 * - If the read was successful, returns the number of bytes read (> 0).
 * - For all other errors, returns -1 and preserves `errno`.
 *
 * @param r The return value from a `read()`-like function (e.g., `read()`, `recv()`).
 * @return
 *   - `>0`  Number of bytes successfully read.
 *   - `0`   No data available yet (non-blocking mode).
 *   - `-1`  Error or EOF; `errno` is set appropriately.
 */
extern ssize_t io_read_result (ssize_t r) ;
extern ssize_t io_write(int fd, char *store, size_t len) ;

/**
 * @brief Normalizes the result of a non-blocking write operation.
 *
 * This function processes the return value of a write-like system call to
 * provide consistent semantics for non-blocking I/O:
 *
 * - If the write failed with `EAGAIN` or `EWOULDBLOCK`, returns 0 and clears `errno`.
 *   This indicates the operation would block and should be retried later.
 *
 * - If the write succeeded (including 0 bytes written), returns the number of bytes sent.
 *   Note: A return of 0 is valid and means no data was queued (e.g., empty buffer),
 *   not an error or disconnection.
 *
 * - For all other errors (including `EPIPE`, `ECONNRESET`, etc.), returns -1
 *   and preserves `errno`.
 *
 * This function does NOT treat zero-byte writes as connection closure — unlike reads,
 * where 0 means EOF. In contrast, `send()` or `write()` returning 0 is legitimate.
 *
 * @param r The return value from a `write()`-like function (e.g., `write()`, `send()`, `sendmsg()`).
 * @return
 *   - `>0`  Number of bytes successfully queued for transmission.
 *   - `0`   No data queued yet (would block); retry when fd becomes writable.
 *   - `-1`  Fatal error occurred; `errno` is set appropriately.
 *
 * @note Callers should use a mechanism like `poll()`, `epoll()`, or `kqueue()` to wait
 *       for writability before retrying when 0 is returned.
 */
extern ssize_t io_write_result(ssize_t r) ;

/**
 * @brief Reads exactly `len` bytes from a BLOCKING fd into `store`.
 *
 * Loops over io_read()/io_read_result() until `len` bytes are read, EOF is
 * reached, or an error occurs. EINTR is handled internally; this function is
 * meant for blocking file descriptors only (on a non-blocking fd it stops on
 * the first EAGAIN and returns a short count with errno == 0).
 *
 * @param fd    A blocking file descriptor.
 * @param store Destination buffer of at least `len` bytes.
 * @param len   Number of bytes to read.
 * @return Number of bytes actually read (0..len).
 *   - `== len` Success: full buffer read.
 *   - `< len`  Short read; inspect errno:
 *       - `EPIPE` EOF reached before `len` bytes.
 *       - other   Read error.
 *       - `0`     Would-block (non-blocking fd passed by mistake).
 */
extern size_t io_allread(int fd, char *store, size_t len) ;

/**
 * @brief Writes exactly `len` bytes from `store` to a BLOCKING fd.
 *
 * Loops over io_write()/io_write_result() until `len` bytes are written or an
 * error occurs. EINTR is handled internally; this function is meant for
 * blocking file descriptors only (on a non-blocking fd it stops on the first
 * EAGAIN and returns a short count with errno == 0).
 *
 * @param fd    A blocking file descriptor.
 * @param store Source buffer of at least `len` bytes.
 * @param len   Number of bytes to write.
 * @return Number of bytes actually written (0..len).
 *   - `== len` Success: full buffer written.
 *   - `< len`  Short write; inspect errno:
 *       - non-zero Write error.
 *       - `0`      Would-block (non-blocking fd passed by mistake).
 */
extern size_t io_allwrite(int fd, char *store, size_t len) ;

#endif
