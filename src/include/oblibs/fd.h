/*
 * fd.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_FD_H
#define OB_FD_H

/**
 * @brief Closes a file descriptor without disturbing `errno`.
 *
 * Equivalent of a plain `close(fd)` save that the current value of `errno`
 * is preserved across the call: it is read before `close()` and restored
 * afterwards. The return value of `close()` is intentionally ignored.
 *
 * Use this in cleanup / error paths where a meaningful `errno` has already
 * been set and must survive the descriptor teardown (e.g. before reporting
 * the error of the operation that actually failed). When you care about the
 * outcome of the close itself, call `close()` directly and check it.
 *
 * This is the oblibs-native replacement for skalibs' `fd_close`; it keeps the
 * exact same name and signature so it acts as a drop-in.
 *
 * @param fd The file descriptor to close.
 */
extern void fd_close(int fd) ;

/**
 * @brief Changes the working directory to the one referred to by `fd`.
 *
 * Wrapper around `fchdir()` that retries on `EINTR`. On success the value of
 * `errno` is preserved (read before the call, restored after); on failure the
 * `errno` set by `fchdir()` is left in place and -1 is returned.
 *
 * This is the oblibs-native replacement for skalibs' `fd_chdir`; it keeps the
 * exact same name and signature so it acts as a drop-in.
 *
 * @param fd A file descriptor referring to a directory.
 * @return 0 on success, -1 on error (with `errno` set).
 */
extern int fd_chdir(int fd) ;

/**
 * @brief Moves the descriptor `from` onto the number `to`.
 *
 * Duplicates `from` onto `to` with `dup2()` (retrying on `EINTR`) then closes
 * `from`, so that the open file description previously reachable through `from`
 * is now reachable through `to` only, with its close-on-exec flag cleared. If
 * `to == from`, no duplication happens and the close-on-exec flag of `to` is
 * simply cleared. On success `errno` is preserved across the call.
 *
 * This is the oblibs-native replacement for skalibs' `fd_move`; it keeps the
 * exact same name and signature so it acts as a drop-in.
 *
 * @param to   The target descriptor number.
 * @param from The source descriptor.
 * @return 0 on success, -1 on error (with `errno` set).
 */
extern int fd_move(int to, int from) ;

#endif
