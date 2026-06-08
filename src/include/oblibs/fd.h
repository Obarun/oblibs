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

/*
 * File-descriptor lifecycle helpers: closing, moving, copying, changing
 * directory through one, toggling close-on-exec, and making sure the standard
 * descriptors are open. These are the oblibs-native replacements for the slice
 * of skalibs `djbunix` that 66 uses (`fd_close`/`fd_chdir`/`fd_move`/`fd_move2`/
 * `fd_copy`/`fd_sanitize`, `coe`/`uncoe`).
 *
 * Naming: `<action>_fd` (e.g. close_fd), because a descriptor is a plain `int`,
 * not an ADT -- a free verb reads better than a pseudo-method, and it sidesteps
 * the skalibs symbol names (no link collision while libskarnet is still linked).
 *
 * Two properties run through the module: on *success* `errno` is preserved
 * (read before, restored after) so a call can sit in a cleanup path without
 * clobbering it; and the interruptible syscalls (`fchdir`, `dup2`) retry on
 * `EINTR`.
 */

/**
 * @brief Closes @fd without disturbing `errno` (the `close()` return is ignored).
 * Use it in cleanup/error paths where a meaningful `errno` must survive the
 * teardown; when you care about the close outcome, call `close()` directly.
 * Replaces skalibs `fd_close`.
 */
extern void close_fd(int fd) ;

/**
 * @brief `fchdir(fd)` with `EINTR` retry; @fd must be a directory. `errno` is
 * preserved on success. Replaces skalibs `fd_chdir`.
 * @return 0 on success, -1 on error (`errno` set).
 */
extern int chdir_fd(int fd) ;

/**
 * @brief Moves descriptor @from onto the number @to: `dup2` (EINTR-retried) then
 * `close(from)`, so the description is reachable through @to only, close-on-exec
 * cleared. `to == from` just clears @to's close-on-exec. `errno` preserved on
 * success. Replaces skalibs `fd_move`.
 * @return 0 on success, -1 on error (`errno` set).
 */
extern int move_fd(int to, int from) ;

/**
 * @brief Moves two descriptors at once: @from1 onto @to1 and @from2 onto @to2,
 * as if `move_fd` were applied to both pairs but safe when the pairs overlap
 * (e.g. swapping 1 and 2, where a naive sequential move would clobber a source).
 * An overlapping source is parked with `dup` and rolled back if the second copy
 * fails. Rejects `from1 == from2` with distinct targets, and `to1 == to2`, with
 * `EINVAL`. `errno` preserved on success. Replaces skalibs `fd_move2`.
 * @return 0 on success, -1 on error (`errno` set).
 */
extern int remap_fds(int to1, int from1, int to2, int from2) ;

/**
 * @brief Duplicates @from onto the number @to (both stay open): `dup2`
 * (EINTR-retried), @to sharing @from's open file description, close-on-exec
 * cleared. `to == from` is rejected with `EINVAL`. `errno` preserved on success.
 * Replaces skalibs `fd_copy`.
 * @return @to (>= 0) on success, -1 on error (`errno` set).
 */
extern int copy_fd(int to, int from) ;

/**
 * @brief Ensures the standard descriptors 0, 1 and 2 are open, opening
 * `/dev/null` on any that is closed (read-only for 0, write-only for 1 and 2);
 * already-open descriptors are left untouched. Guards against a later `open()`
 * silently grabbing a standard descriptor number. Replaces skalibs `fd_sanitize`.
 * @return 1 on success, 0 on error (`errno` set).
 */
extern int ensure_stdfds(void) ;

/**
 * @brief Sets the close-on-exec flag on @fd (`EINTR`-safe). Replaces skalibs `coe`.
 * @return 1 on success, 0 on error (`errno` set).
 */
extern int cloexec_fd(int fd) ;

/**
 * @brief Clears the close-on-exec flag on @fd (`EINTR`-safe). Replaces skalibs `uncoe`.
 * @return 1 on success, 0 on error (`errno` set).
 */
extern int uncloexec_fd(int fd) ;

#endif
