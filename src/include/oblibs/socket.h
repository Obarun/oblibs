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
 * except according to the terms contained in the LICENSE file.
 *
 * @brief Unix Domain Socket Utilities
 *
 * This file provides a set of helper functions and macros for creating,
 * binding, connecting, accepting, and managing Unix domain stream sockets
 * with support for common flags such as non-blocking I/O and close-on-exec.
 *
 * The implementation wraps standard POSIX socket APIs with convenience
 * functions that handle common error cases (e.g., EINTR retries) and
 * abstract flag handling via `O_NONBLOCK` and `O_CLOEXEC`.
 */

#ifndef OB_SOCKET_H
#define OB_SOCKET_H

#include <fcntl.h>
#include <sys/socket.h>
#include <oblibs/io.h>

/**
 * @brief Default backlog for listening sockets.
 *
 * Uses `SOMAXCONN` to allow the kernel to choose a reasonable maximum.
 */
#define SOCKETUNIX_BACKLOG SOMAXCONN
/**
 * @brief Maximum length for a Unix socket path.
 *
 * Defined as 107 to leave room for the null terminator within the
 * `sun_path` field (which is typically 108 bytes on most systems).
 */
#define SOCKETUNIX_PATH_MAX 107 // 108 - 1

/**
 * @brief Create a socket with optional `SOCK_NONBLOCK` and `SOCK_CLOEXEC`.
 *
 * Creates a socket using the specified domain, type, and protocol,
 * and applies non-blocking and close-on-exec behavior based on the
 * provided `flags` (interpreted as `O_NONBLOCK` and `O_CLOEXEC`).
 *
 * @param[in] domain   Socket domain (e.g., `AF_UNIX`).
 * @param[in] type     Socket type (e.g., `SOCK_STREAM`).
 * @param[in] protocol Protocol (usually 0).
 * @param[in] flags    Optional flags: `O_NONBLOCK`, `O_CLOEXEC`.
 * @return File descriptor on success, -1 on error (errno set).
 *
 * @note This function uses `socket()` with `SOCK_NONBLOCK` and `SOCK_CLOEXEC`
 *       if supported by the platform (Linux ≥2.6.27, etc.).
 */
extern int socket_private(int domain, int type, int protocol, unsigned int flags) ;

/**
 * @brief Set file descriptor flags (e.g., `FD_CLOEXEC`).
 *
 * Atomically sets the specified file descriptor flags using `fcntl()`.
 *
 * @param[in] fd     File descriptor.
 * @param[in] flags  Flags to set (e.g., `FD_CLOEXEC`).
 * @return 1 on success, 0 on failure.
 */
int socket_setfd_flags(int fd, int flags) ;

/**
 * @brief Clear file descriptor flags.
 *
 * Atomically clears the specified file descriptor flags using `fcntl()`.
 *
 * @param[in] fd     File descriptor.
 * @param[in] flags  Flags to clear (e.g., `FD_CLOEXEC`).
 * @return 1 on success, 0 on failure.
 */
int socket_unsetfd_flags(int fd, int flags) ;

/**
 * @brief Set the close-on-exec flag (`FD_CLOEXEC`) on a file descriptor.
 *
 * Convenience macro using `socket_setfd()`.
 *
 * @param[in] fd File descriptor.
 * @return 1 on success, 0 on failure.
 *
 * @see socket_setfd
 */
#define socket_set_coe(fd) socket_setfd(fd, FD_CLOEXEC)

/**
 * @brief Set the non-blocking I/O flag (`O_NONBLOCK`) on a file descriptor.
 *
 * Uses an external `io_setfl()` function (assumed to be defined elsewhere).
 *
 * @param[in] fd File descriptor.
 * @return Result of `io_setfl(fd, O_NONBLOCK)`.
 *
 */
#define socket_set_nonblock(fd) io_setfl(fd, O_NONBLOCK)

/**
 * @brief Create a Unix domain stream socket with optional flags.
 *
 * This macro wraps socket_private() to create an `AF_UNIX`, `SOCK_STREAM`
 * socket. Supported flags: `O_NONBLOCK`, `O_CLOEXEC`.
 *
 * @param[in] flags Optional flags (`O_NONBLOCK`, `O_CLOEXEC`).
 * @return File descriptor on success, -1 on error (errno set).
 *
 * @see socket_private
 */
#define socketunix_create(flags) socket_private(AF_UNIX, SOCK_STREAM, 0, flags)
/**
 * @brief Start listening on a bound Unix socket.
 *
 * Wraps the standard `listen()` system call.
 *
 * @param[in] socket File descriptor of the bound socket.
 * @param[in] backlog Maximum number of pending connections.
 * @return 0 on success, -1 on error (errno set).
 */
#define socketunix_listen(socket, backlog) listen(socket, backlog)

/**
 * @brief Bind a Unix domain socket to a filesystem path.
 *
 * Binds the given socket file descriptor to the specified path.
 * Validates path length against `SOCKETUNIX_PATH_MAX`.
 *
 * @param[in] fd   Socket file descriptor.
 * @param[in] path Null-terminated filesystem path (must not be NULL).
 * @return 0 on success, -1 on error (errno set to `EINVAL`, `EPROTO`, etc.).
 */
extern int socketunix_bind(int fd, const char *path) ;

/**
 * @brief Accept a connection on a Unix domain socket.
 *
 * Accepts an incoming connection and returns a new file descriptor.
 * Automatically retries on `EINTR`. Supports `O_NONBLOCK` and `O_CLOEXEC`
 * via `accept4()` if available.
 *
 * @param[in] s      Listening socket file descriptor.
 * @param[in] flags  Optional flags: `O_NONBLOCK`, `O_CLOEXEC`.
 * @return New connected socket file descriptor on success, -1 on error.
 *
 * @note On systems without `accept4()`, this would need emulation,
 *       but this implementation assumes `accept4()` is available.
 */
extern int socketunix_accept(int s, unsigned int flags) ;

/**
 * @brief Connect a Unix domain socket to a remote path.
 *
 * Connects the given socket to the specified Unix socket path.
 * Validates path length and constructs the `sockaddr_un` properly.
 *
 * @param[in] fd   Socket file descriptor.
 * @param[in] path Null-terminated target path (must not be NULL).
 * @return 0 on success, -1 on error (errno set, e.g., `ENAMETOOLONG`).
 */
extern int socketunix_connect(int fd, const char *path) ;

/**
 * @brief Retrieve peer credentials for a connected Unix socket.
 *
 * Obtains the `struct ucred` (PID, UID, GID) of the peer process
 * via the `SO_PEERCRED` socket option.
 *
 * @param[in]  fd   Connected Unix socket file descriptor.
 * @param[out] cred Pointer to `struct ucred` to fill.
 * @return 0 on success, -1 on error (errno set).
 *
 * @note Only works on connected `AF_UNIX` stream sockets on Linux.
 * @see struct ucred
 */
extern int socketunix_getucred(int fd, struct ucred *cred) ;

#endif
