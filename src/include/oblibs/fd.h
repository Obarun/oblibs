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
 * except according to the terms contained in the LICENSE file.
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
 * @brief Close a descriptor without disturbing `errno`.
 *
 * Calls `close(fd)` and ignores its return value, saving `errno` before the
 * call and restoring it after. Use it in cleanup/error paths where a meaningful
 * `errno` must survive the teardown; when you care about the close outcome, call
 * `close()` directly. Replaces skalibs `fd_close`.
 *
 * @param[in] fd Descriptor to close. A negative or invalid value is passed to
 *               `close()` as is; its failure is ignored.
 *
 * @return Nothing. `errno` is left exactly as it was on entry, regardless of
 *         whether `close()` succeeded.
 */
extern void close_fd(int fd) ;

/**
 * @brief Change the working directory to a descriptor.
 *
 * Calls `fchdir(fd)`, retrying while it fails with `EINTR`. @fd must refer to an
 * open directory. On success `errno` is restored to the value it held on entry.
 * Replaces skalibs `fd_chdir`.
 *
 * @param[in] fd Descriptor of an open directory.
 *
 * @return 0 on success; `errno` is preserved (restored to its entry value).
 * @return -1 on error, with `errno` set to the `fchdir(2)` errno (e.g. `EBADF`
 *         if @fd is not a valid open descriptor, `ENOTDIR` if @fd is not a
 *         directory, `EACCES` if search permission was denied).
 */
extern int chdir_fd(int fd) ;

/**
 * @brief Move a descriptor onto another number.
 *
 * Renumbers @from to @to: `dup2(from, to)` (retried while it fails with
 * `EINTR`), then `close(from)` via `close_fd()`. After success the open file
 * description is reachable through @to only, with @to's close-on-exec flag
 * cleared (a side effect of `dup2`). The special case `to == from` performs no
 * `dup2`/`close`; it only clears @to's close-on-exec via `uncloexec_fd()`. On
 * success `errno` is restored to its entry value. Replaces skalibs `fd_move`.
 *
 * @param[in] to   Target descriptor number @from is moved onto.
 * @param[in] from Source descriptor; closed after a successful move (unless
 *                 @to == @from).
 *
 * @return 0 on success; `errno` is preserved.
 * @return -1 on error. When @to != @from, `errno` is the `dup2(2)` errno (e.g.
 *         `EBADF` if @from is invalid, `EINVAL` for an out-of-range @to); when
 *         @to == @from, `errno` is the `fcntl(2)` `F_GETFD`/`F_SETFD` errno left
 *         by the failing `uncloexec_fd()`. @from is not closed when an error is
 *         returned.
 * @see uncloexec_fd
 * @see close_fd
 */
extern int move_fd(int to, int from) ;

/**
 * @brief Move two descriptors at once, safely when the pairs overlap.
 *
 * Renumbers @from1 onto @to1 and @from2 onto @to2 as if `move_fd()` were applied
 * to both pairs, but tolerates overlap between the two pairs (e.g. swapping 1
 * and 2, where a naive sequential move would clobber a source). When @from2
 * would be clobbered by the first move (@from2 == @to1), it is first parked with
 * `dup()`; the two copies are done with `copy_fd()`, and if the second copy
 * fails the first target is closed and the parked descriptor is rolled back onto
 * @from2 before returning. The original source descriptors are closed at the end
 * (each unless it coincides with the other pair's target). On success `errno` is
 * restored to its entry value. Replaces skalibs `fd_move2`.
 *
 * Degenerate pairs are forwarded to `move_fd()`: if @to1 == @from1 only the
 * second pair is moved; if @to2 == @from2 only the first pair is moved; if
 * @from1 == @from2 with @to1 == @to2 the single pair is moved.
 *
 * @param[in] to1   Target number for @from1.
 * @param[in] from1 First source descriptor.
 * @param[in] to2   Target number for @from2.
 * @param[in] from2 Second source descriptor.
 *
 * @return 0 on success; `errno` is preserved.
 * @return -1 on error.
 * @retval -1 `errno` is set to `EINVAL` if @from1 == @from2 while @to1 != @to2,
 *         or if @to1 == @to2 while @from1 != @from2 (incoherent remap).
 * @retval -1 Otherwise `errno` is the errno left by the underlying primitive:
 *         the `dup(2)` errno when parking @from2 fails, or the `copy_fd()` /
 *         `move_fd()` errno of a failing renumber (ultimately a `dup2(2)` or
 *         `fcntl(2)` errno).
 * @see move_fd
 * @see copy_fd
 */
extern int remap_fds(int to1, int from1, int to2, int from2) ;

/**
 * @brief Duplicate a descriptor onto another number, both staying open.
 *
 * Calls `dup2(from, to)` (retried while it fails with `EINTR`). On success @to
 * refers to the same open file description as @from, with @to's close-on-exec
 * flag cleared (a side effect of `dup2`); @from stays open. The case
 * `to == from` is rejected up front. On success `errno` is restored to its entry
 * value. Replaces skalibs `fd_copy`.
 *
 * @param[in] to   Target descriptor number that becomes a duplicate of @from.
 * @param[in] from Source descriptor; left open.
 *
 * @return @to (>= 0) on success; `errno` is preserved. (`dup2` returns @to, so
 *         the value returned equals @to.)
 * @return -1 on error.
 * @retval -1 `errno` is set to `EINVAL` if @to == @from.
 * @retval -1 Otherwise `errno` is the `dup2(2)` errno (e.g. `EBADF` if @from is
 *         not a valid open descriptor, `EINVAL` for an out-of-range @to).
 */
extern int copy_fd(int to, int from) ;

/**
 * @brief Make sure the standard descriptors 0, 1 and 2 are open.
 *
 * Probes each of the descriptors 2, 1 and 0 (in that order) with
 * `fcntl(fd, F_GETFD)`. A descriptor that is already open is left untouched. A
 * descriptor that is closed (probe fails with `EBADF`) is reopened by opening
 * `/dev/null` and moving it onto the standard number with `move_fd()`:
 * read-only for descriptor 0, write-only for descriptors 1 and 2. This guards
 * against a later `open()` silently grabbing a standard descriptor number.
 * Replaces skalibs `fd_sanitize`.
 *
 * @return 1 on success (all three of 0, 1, 2 are open on return).
 * @return 0 on error.
 * @retval 0 `errno` is set to the value left by the first failing step: the
 *         `fcntl(2)` errno if a probe fails with something other than `EBADF`;
 *         otherwise the `open(2)` errno if opening `/dev/null` fails, or the
 *         `move_fd()` (`dup2(2)`/`fcntl(2)`) errno if renumbering it onto the
 *         standard descriptor fails. The reopened `/dev/null` descriptor is
 *         closed before returning on a `move_fd()` failure.
 * @see move_fd
 */
extern int ensure_stdfds(void) ;

/**
 * @brief Set the close-on-exec flag on a descriptor.
 *
 * Reads the descriptor flags with `fcntl(fd, F_GETFD)` and writes them back with
 * `FD_CLOEXEC` added via `fcntl(fd, F_SETFD, ...)`. Replaces skalibs `coe`.
 *
 * @param[in] fd Descriptor whose close-on-exec flag is set.
 *
 * @return 1 on success.
 * @return 0 on error, with `errno` set to the `fcntl(2)` errno left by the
 *         failing `F_GETFD` or `F_SETFD` (e.g. `EBADF` if @fd is not a valid open
 *         descriptor). `errno` is not preserved on this path.
 * @note `F_GETFD`/`F_SETFD` do not block, so there is no `EINTR` to retry here
 *       (unlike `chdir_fd`/`move_fd`, whose syscalls are interruptible).
 * @see uncloexec_fd
 */
extern int cloexec_fd(int fd) ;

/**
 * @brief Clear the close-on-exec flag on a descriptor.
 *
 * Reads the descriptor flags with `fcntl(fd, F_GETFD)` and writes them back with
 * `FD_CLOEXEC` removed via `fcntl(fd, F_SETFD, ...)`. Replaces skalibs `uncoe`.
 *
 * @param[in] fd Descriptor whose close-on-exec flag is cleared.
 *
 * @return 1 on success.
 * @return 0 on error, with `errno` set to the `fcntl(2)` errno left by the
 *         failing `F_GETFD` or `F_SETFD` (e.g. `EBADF` if @fd is not a valid open
 *         descriptor). `errno` is not preserved on this path.
 * @note `F_GETFD`/`F_SETFD` do not block, so there is no `EINTR` to retry here
 *       (unlike `chdir_fd`/`move_fd`, whose syscalls are interruptible).
 * @see cloexec_fd
 */
extern int uncloexec_fd(int fd) ;

/**
 * @brief Take an advisory whole-file lock on a descriptor.
 *
 * Calls `flock(2)` on @fd: an exclusive (write) lock when @ex is set, a shared
 * (read) lock otherwise; with `LOCK_NB` when @nb is set, so the call does not
 * block. A blocking wait interrupted by a signal (`EINTR`) is retried. The lock
 * is owned by the open file description, so it is released when the last
 * descriptor referring to that description is closed -- in particular an
 * `O_CLOEXEC` descriptor releases it automatically at the next `exec`. On
 * success `errno` is restored to its entry value. Replaces skalibs `fd_lock`.
 *
 * @param[in] fd Descriptor to lock.
 * @param[in] ex Non-zero for an exclusive (write) lock; zero for a shared (read)
 *               lock.
 * @param[in] nb Non-zero to request the lock non-blocking; zero to wait for it.
 *
 * @return 1 if the lock was taken; `errno` is preserved.
 * @return 0 if @nb is set and the lock is already held by an incompatible owner
 *         (`flock` returned `EWOULDBLOCK`); contended, no lock taken.
 * @return -1 on error, with `errno` set to the `flock(2)` errno (e.g. `EBADF` if
 *         @fd is not an open descriptor, `ENOLCK` if the lock table is full).
 *         `EINTR` does not reach the caller; it is retried internally.
 */
extern int lock_fd(int fd, int ex, int nb) ;

#endif
