/*
 * exec.h
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
 * @brief Process-image with PATH resolution.
 *
 * Every function in this header REPLACES the current process image; it never
 * forks. On success it does not return. When PATH resolution is performed, it
 * walks the PATH of the CURRENT environment (`getenv("PATH")`, falling back to
 * `EXEC_DEFAULTPATH`), which is independent of the `envp` handed to the
 * executed program.
 */

#ifndef OB_EXEC_H
#define OB_EXEC_H

#include <stddef.h>

#include <oblibs/attributes.h>

/**
 * @brief Fallback search path used when PATH is unset in the current environment.
 *
 * When `file` carries no '/', the search directories are taken from
 * `getenv("PATH")`; if that returns NULL, this value (`"/usr/bin:/bin"`) is
 * used instead. The constant is consulted only as a fallback and only for the
 * PATH walk; it never overrides an existing (even empty) PATH variable.
 */
#define EXEC_DEFAULTPATH "/usr/bin:/bin"

/**
 * @brief Replace the current process image with @file, run with @argv and @envp.
 *
 * If @file contains a '/', it is `execve`'d verbatim and no PATH search is
 * performed. Otherwise @file is looked up in the ':'-separated directories of
 * the CURRENT environment's PATH (`getenv("PATH")`, or `EXEC_DEFAULTPATH` when
 * PATH is unset), trying `execve` on `<dir>/<file>` for each directory in turn.
 * The PATH that drives this lookup is the caller's, not the @envp passed to the
 * executed program.
 *
 * During the walk, a per-directory `execve` failing with `ENOENT` is not fatal:
 * the search continues to the next directory. A failure with `EACCES`, `EPERM`,
 * `EISDIR` or `ENOTDIR` is remembered as the candidate error but the search
 * still continues; any other failure stops the search immediately. When the
 * walk ends, errno is set to the most meaningful remembered error if one was
 * recorded, otherwise it carries the errno left by the last `execve` attempt
 * (`ENOENT` when no directory contained @file). Empty PATH components (a leading,
 * trailing or doubled ':') are skipped without an `execve` attempt.
 *
 * This function never forks and, on success, never returns: the calling image is
 * gone. It returns to the caller ONLY on failure.
 *
 * @param[in] file Program to execute. Must not be NULL. A '/' selects the
 *                 verbatim path; otherwise it is a bare name resolved via PATH.
 * @param[in] argv NULL-terminated argument vector passed to the program. Must
 *                 not be NULL.
 * @param[in] envp NULL-terminated environment handed to the executed program.
 *                 Must not be NULL.
 *
 * @return Does not return on success (the process image is replaced).
 * @return Returns to the caller only on failure, with errno set to the
 *         underlying `execve` errno selected as described above (e.g. `ENOENT`
 *         when @file was not found anywhere in PATH, `EACCES` when found but not
 *         executable, or any other errno `execve(2)` may set).
 *
 * @note PATH resolution is performed only when @file contains no '/'. When a '/'
 *       is present, a single `execve(@file, ...)` is issued and its errno is
 *       reported verbatim.
 */
extern void exec_path (char const *file, char const *const *argv, char const *const *envp) ;

/**
 * @brief Replace the current process image with @file after merging @modif into @envp.
 *
 * Behaves exactly like exec_path(), except that the environment seen by the
 * executed program is first computed by merging the modifier block @modif into
 * @envp through `environ_merge`. @modif is a buffer of @modiflen bytes holding a
 * sequence of null-terminated records: `"KEY=value"` sets (or replaces) a
 * variable, a bare `"KEY"` (no '=') unsets it. The merged environment is built
 * on the stack, sized for the worst case (every modifier being an addition), so
 * the merge never overflows and the result is always a valid NULL-terminated
 * vector. The resolved environment is then passed to exec_path().
 *
 * PATH resolution still uses the CURRENT environment's PATH, not the merged
 * @envp. This function never forks and, on success, never returns.
 *
 * @param[in] file    Program to execute. Must not be NULL. '/' selects a
 *                    verbatim path; a bare name is resolved via PATH.
 * @param[in] argv    NULL-terminated argument vector. Must not be NULL.
 * @param[in] envp    NULL-terminated base environment. Must not be NULL.
 * @param[in] modif   Modifier block: a concatenation of null-terminated
 *                    `"KEY=value"` / `"KEY"` records. May be empty (@modiflen 0).
 * @param[in] modiflen Length of @modif in bytes.
 *
 * @return Does not return on success (the process image is replaced).
 * @return Returns to the caller only on failure, with errno set to the
 *         underlying `execve` errno (see exec_path()).
 *
 * @see exec_path
 */
extern void exec_path_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen) ;

/**
 * @brief Replace the current process image with @file, or terminate the process on failure.
 *
 * Calls exec_path() and, if it returns (i.e. the exec failed), terminates the
 * process via `log_dieusys`, which emits a system error diagnostic ("exec
 * <file>: <strerror>") and exits. The exit code follows the POSIX shell
 * convention: 127 when errno is `ENOENT` (command not found), 126 for any other
 * errno (found but not executable, permission denied, etc.).
 *
 * This function never forks and never returns: on success the image is replaced,
 * on failure the process exits.
 *
 * @param[in] file Program to execute. Must not be NULL.
 * @param[in] argv NULL-terminated argument vector. Must not be NULL.
 * @param[in] envp NULL-terminated environment for the executed program. Must not
 *                 be NULL.
 *
 * @return Never returns. On exec failure the process exits 127 if errno is
 *         `ENOENT`, otherwise 126.
 *
 * @see exec_path
 */
extern void exec_path_die (char const *file, char const *const *argv, char const *const *envp) attribute_noreturn ;

/**
 * @brief Merge @modif into @envp, replace the process image, or terminate on failure.
 *
 * Calls exec_path_merge() and, if it returns (i.e. the exec failed), terminates
 * the process via `log_dieusys`: it prints "exec <file>: <strerror>" and exits
 * 127 when errno is `ENOENT`, 126 otherwise, matching the POSIX shell
 * convention. The @modif semantics are those of exec_path_merge().
 *
 * This function never forks and never returns.
 *
 * @param[in] file    Program to execute. Must not be NULL.
 * @param[in] argv    NULL-terminated argument vector. Must not be NULL.
 * @param[in] envp    NULL-terminated base environment. Must not be NULL.
 * @param[in] modif   Modifier block of null-terminated `"KEY=value"` / `"KEY"`
 *                    records. May be empty (@modiflen 0).
 * @param[in] modiflen Length of @modif in bytes.
 *
 * @return Never returns. On exec failure the process exits 127 if errno is
 *         `ENOENT`, otherwise 126.
 *
 * @see exec_path_merge
 */
extern void exec_path_merge_die (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen) attribute_noreturn ;

#endif
