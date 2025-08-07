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

#include <sys/types.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0x40000000
#endif

extern int io_coe(int fd) ;
extern int io_uncoe(int fd) ;
extern int io_set_flags(int fd, int flags) ;
extern int io_unset_flags(int fd, int flags) ;
extern int io_set_block(int fd) ; // ndelay_off
extern int io_set_nonblock(int fd) ; // ndelay_on
extern ssize_t io_open(const char *path, unsigned int flags) ;
extern ssize_t io_read(char *store, int fd, size_t len) ;

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
extern ssize_t io_read_exit (ssize_t r) ;
extern ssize_t io_write(char *store, int fd, size_t len) ;

#endif
