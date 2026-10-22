/*
 * directory.h
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * @brief Filesystem directory and hierarchy utilities.
 *
 * This module provides helpers to create, destroy, copy and inspect filesystem
 * directories and the path strings that name them. It groups three families:
 * directory creation (`dir_create`, `dir_create_parent`, `dir_create_tmp`),
 * recursive whole-tree operations (`dir_destroy` for `rm -rf`, `tree_copy` for
 * `cp -a`), and pure path-string helpers (`dir_beabsolute`, `dir_is_child`,
 * `dir_unslash`), plus an errno-preserving `closedir` wrapper (`dir_close`).
 *
 * Creation helpers require absolute paths and treat an existing target as
 * success where noted. The recursive helpers slurp each directory listing in
 * full and close the directory handle before recursing, so no `DIR *` is held
 * open across the recursion.
 */


#ifndef OB_DIRECTORY_H
#define OB_DIRECTORY_H

#include <sys/types.h>
#include <dirent.h>

#include <oblibs/strbuf.h>

/**
 * @brief Close a directory stream without disturbing errno.
 *
 * Wraps `closedir()` for cleanup paths: it saves the current `errno`, calls
 * `closedir()` on @dir, and restores `errno` to its saved value, so a failing
 * close cannot clobber the error a caller is about to report. Any error from
 * `closedir()` is intentionally discarded. There is no `EINTR` retry: `closedir`
 * may already have freed the `DIR` object, so a retry would risk a use-after-free.
 *
 * @param[in] dir Directory stream to close. Must be a valid `DIR *` returned by
 *                `opendir()`; it is not NULL-checked and is passed to `closedir()`
 *                unconditionally.
 *
 * @return Nothing. `errno` is left exactly as it was on entry.
 */
extern void dir_close (DIR *dir) ;

/**
 * @brief Create a single directory at an absolute path.
 *
 * Calls `mkdir(@dst, @mode)`. @dst must be absolute; the parent directory must
 * already exist (this does NOT create intermediate components -- see
 * dir_create_parent()). On success `errno` is restored to the value it had on
 * entry, so a successful call never changes `errno`.
 *
 * @param[in] dst  Absolute directory path to create. Must not be NULL (it is
 *                 dereferenced without a NULL check) and must begin with '/'.
 * @param[in] mode Permission bits for the new directory, as passed to `mkdir()`
 *                 (subject to the process umask).
 *
 * @return Status code:
 *      - 1 on success; `errno` is restored to its entry value.
 *      - 0 on failure; `errno` is set to indicate the error.
 *
 * @retval 0 errno is set to `ENOTDIR` if @dst does not begin with '/'.
 * @retval 0 errno is set to the `mkdir()` errno otherwise (e.g. `EEXIST` if @dst
 *         already exists, `ENOENT` if the parent does not exist, `EACCES`).
 *
 * @see dir_create_parent
 */
extern int dir_create (char const *dst, mode_t mode) ;

/**
 * @brief Create a uniquely-named temporary directory under an absolute path.
 *
 * Builds the template string `"<dst>/<tname>:XXXXXX"` into the caller-provided
 * @sb strbuf, then calls `mkdtemp()` on it. `mkdtemp()` replaces the trailing
 * `XXXXXX` in place with a unique suffix and creates the directory with mode
 * 0700. On success the final path lives in @sb's buffer and is returned as a
 * pointer to that buffer.
 *
 * @param[in,out] sb    strbuf used to build the template and hold the result. On
 *                      success it contains the created directory's path. Ownership
 *                      of the buffer remains with the caller (it is not freshly
 *                      allocated by this function); the caller frees/cleans @sb.
 * @param[in]     dst   Absolute parent path under which the temporary directory is
 *                      created. Must not be NULL (dereferenced without a NULL
 *                      check) and must begin with '/'.
 * @param[in]     tname Name component placed before the random suffix. Must not be
 *                      NULL.
 *
 * @return On success, a pointer to the NUL-terminated created-directory path
 *         inside @sb's buffer (`sb->s`); the caller does NOT free this pointer
 *         separately -- it is owned by @sb. On failure, NULL.
 *
 * @retval NULL errno is set to `EINVAL` if @dst does not begin with '/'.
 * @retval NULL errno is set (e.g. `ENOMEM`) if building the template into @sb
 *         fails to allocate.
 * @retval NULL errno is set to the `mkdtemp()` errno if directory creation fails.
 */
extern char *dir_create_tmp(strbuf *sb, char const *dst, char const *tname) ;

/**
 * @brief Create a directory and all missing parent components (`mkdir -p`).
 *
 * Walks @dst left to right and, at each '/' separator (and finally at the full
 * path), calls `mkdir()` with @mode, tolerating `EEXIST` so already-present
 * components are accepted. Every directory it actually creates -- intermediate
 * and final -- uses @mode (subject to the umask); existing components keep their
 * current mode.
 *
 * @param[in] dst  Directory path to create, with intermediate components. Must
 *                 not be NULL (it is dereferenced without a NULL check). A
 *                 relative path is created relative to the current directory; a
 *                 leading '/' is skipped as a separator.
 * @param[in] mode Permission bits applied to each directory this call creates.
 *
 * @return Status code:
 *      - 1 on success, including the case where @dst already exists.
 *      - 0 on failure; `errno` is set to indicate the error.
 *
 * @retval 0 errno is set to the failing `mkdir()` errno of the first component
 *         that cannot be created and does not already exist (e.g. `ENOTDIR` when
 *         an intermediate component exists as a non-directory, `EACCES`,
 *         `ENOSPC`).
 *
 * @note If @dst itself already exists -- even as a regular file, symlink or other
 *       non-directory -- `mkdir()` returns `EEXIST`, which this function tolerates,
 *       so the call returns 1 (success) WITHOUT verifying that @dst is a
 *       directory. The caller must `stat()` separately if that guarantee matters.
 * @see dir_create
 */
extern int dir_create_parent(char const *dst, mode_t mode) ;

/**
 * @brief Resolve @dir to an absolute path and write it into @dst.
 *
 * Opens the current directory to remember it, `chdir()`s into @dir, reads the
 * resulting absolute path with `getcwd()`, then `chdir()`s back. If @dir is a
 * non-directory (`chdir` fails with `ENOTDIR`), the absolute path of the current
 * directory is taken instead and @dir is appended to it as a trailing component,
 * yielding the absolute path of the file. @dst always ends up NUL-terminated (by
 * `getcwd()` and the final string build).
 *
 * @param[out] dst Output buffer receiving the NUL-terminated absolute path. MUST
 *                 be at least 4096 bytes: `getcwd()` is called with a hardcoded
 *                 length of 4096, and for the non-directory case @dir is then
 *                 appended after the retrieved path. Must not be NULL.
 * @param[in]  dir Path to resolve. May be relative or absolute. Must not be NULL.
 *
 * @return Status code:
 *      - 1 on success; @dst holds the absolute path. `errno` is restored to its
 *        entry value.
 *      - 0 on failure; @dst is left in an unspecified state and `errno` is set to
 *        the errno of the failing step (`io_open(".")`, `chdir(@dir)`, `getcwd()`
 *        or the `chdir` back).
 *
 * @note The current working directory is changed during the call and restored
 *       before returning; this is not safe to run concurrently with other
 *       threads that depend on the process cwd.
 */
extern int dir_beabsolute(char *dst, char const *dir) ;

/**
 * @brief Test whether @child names a path strictly below @parent (string check).
 *
 * This is a pure path-string comparison; it performs no filesystem access and
 * never touches `errno`. @child is considered a child of @parent when @child
 * begins with @parent and the very next character in @child is a '/' separator,
 * so that @parent matches on a whole path component (a trailing '/' on @parent is
 * accounted for). Both paths must be absolute. Equal paths are NOT children.
 *
 * @param[in] parent Absolute candidate ancestor path. Must not be NULL and must
 *                   begin with '/'.
 * @param[in] child  Absolute candidate descendant path. Must not be NULL and must
 *                   begin with '/'.
 *
 * @return Status code:
 *      - 1 if @child is strictly under @parent.
 *      - 0 otherwise. `errno` is NOT set; 0 means only "not a child" (including
 *        the cases where either path is not absolute, @child does not have
 *        @parent as a prefix, or @parent matches only part of a component).
 */
extern int dir_is_child(char const *parent, char const *child) ;

/**
 * @brief Strip a single trailing '/' from @path in place.
 *
 * If @path has length greater than 1 and its last character is '/', that '/' is
 * overwritten with a NUL. Exactly one trailing slash is removed: a path ending in
 * multiple slashes (e.g. "/a///") loses only the last one ("/a//").
 *
 * @param[in,out] path NUL-terminated path mutated in place. Must not be NULL. A
 *                     path of length 0 or 1 (including "" and the root "/") is
 *                     left unchanged.
 *
 * @return Nothing. `errno` is not touched.
 */
extern void dir_unslash(char *path) ;

/**
 * @brief Recursively remove @dir and everything under it (`rm -rf` semantics).
 *
 * Tries to `unlink()` @dir first; if that fails because it is a directory
 * (`EISDIR`/`EPERM`), the whole directory listing is read into memory and the
 * directory is closed BEFORE recursing into each entry, so no `DIR *` is held
 * open across the recursion; the now-empty directory is then removed with
 * `rmdir()`. Symlinks are unlinked, never followed. Removing a path that does not
 * exist (`ENOENT`) is treated as success.
 *
 * @param[in] dir Path to remove. Must not be NULL.
 *
 * @return Status code:
 *      - 1 on success, including the case where @dir did not exist.
 *      - 0 on failure; `errno` is set to indicate the error.
 *
 * @retval 0 errno is set to `ENOMEM` if a working buffer cannot be allocated.
 * @retval 0 errno is set to the failing `unlink()`, `opendir()`, `readdir()` or
 *         `rmdir()` errno of the entry that could not be removed (e.g. `EACCES`,
 *         `EROFS`, `ENOTEMPTY`).
 */
extern int dir_destroy (char const *dir) ;

/**
 * @brief Replace the directory @dst by the directory @src.
 *
 * Nothing is copied, @src becomes @dst and the previous @dst is removed
 * entirely. @src must be prepared on the same filesystem as @dst, and @tmpdir
 * too. Steps:
 *   1. creates in @tmpdir an empty directory of its own, named after the last
 *      component of @dst, with dir_create_tmp();
 *   2. `rename(dst, <that directory>)`, the previous @dst leaves its place. If
 *      @dst does not exist (`ENOENT`), the empty directory is removed and the
 *      result of file_tmp_rename(@src, @dst) is returned;
 *   3. file_tmp_rename(@src, @dst). On failure, the previous @dst is put back by
 *      the reverse `rename()`;
 *   4. on success, the previous @dst is removed with dir_destroy().
 *
 * Between steps 2 and 3, @dst briefly does not exist: a concurrent reader can
 * see it missing. Nothing is logged, the caller decides what to report.
 *
 * @param[in] tmpdir Absolute directory receiving the previous @dst during the
 *                   swap. Must not be NULL, must begin with '/'.
 * @param[in] src    Directory to put in place. Must not be NULL.
 * @param[in] dst    Directory to replace. Must not be NULL and must contain a
 *                   '/' (its last component names the temporary).
 *
 * @return 1 on success, @dst is now what @src was. The previous @dst is removed;
 *         if that removal fails, 1 is still returned and the previous @dst stays
 *         in @tmpdir, for the caller to clean @tmpdir at its next opportunity.
 * @return 0 on failure.
 * @retval 0 `errno` is set by ob_basename() (`EINVAL` if @dst has no '/') or by
 *         dir_create_tmp() (`EINVAL` if @tmpdir is not absolute, `ENOMEM`, the
 *         `mkdtemp(3)` errno such as `ENOENT` if @tmpdir does not exist).
 *         Nothing was moved.
 * @retval 0 `errno` is set to the `rename(2)` errno of step 2, other than
 *         `ENOENT` (e.g. `EXDEV`, `EISDIR` if @dst is not a directory). The
 *         empty directory is removed, @dst is untouched.
 * @retval 0 `errno` is set by file_tmp_rename() (e.g. `ENOENT` if @src does not
 *         exist). When @dst did not exist, nothing else changed. Otherwise the
 *         previous @dst is put back; if putting it back fails too, it stays in
 *         @tmpdir and @dst does not exist.
 * @see file_tmp_rename
 * @see dir_create_tmp
 */
extern int dir_replace_at(char const *tmpdir, char const *src, char const *dst) ;

/**
 * @brief Recursively copy the hierarchy at @src to @dst (`cp -a` semantics).
 *
 * Copies the node at @src to @dst, dispatching on its `lstat()` type and
 * preserving mode and ownership: regular files (contents copied, destination
 * truncated if present), directories (created, then recursed; an existing
 * destination directory is tolerated, a non-directory in its place fails with
 * `ENOTDIR`), FIFOs, symlinks (copied verbatim by their target, never followed;
 * an existing destination symlink is replaced), and character/block device and
 * socket nodes (recreated with `mknod()`). Each directory listing is read fully
 * and the directory closed before recursing, so no `DIR *` is held open across
 * the recursion. Ownership is applied best-effort with `lchown()` (errors
 * ignored); a child copy failure is propagated. Native replacement for skalibs
 * hiercopy.
 *
 * @param[in] src Source path to copy. Must not be NULL.
 * @param[in] dst Destination path. Must not be NULL. Its parent directory must
 *                already exist; @dst itself may already exist (see above).
 *
 * @return Status code:
 *      - 1 on success.
 *      - 0 on failure; `errno` is set to indicate the error.
 *
 * @retval 0 errno is set to `ENOMEM` if a working buffer cannot be allocated.
 * @retval 0 errno is set to `ENOTDIR` if @dst exists as a non-directory where a
 *         directory must be created.
 * @retval 0 errno is set to `ENOTSUP` if @src is of a type this function does not
 *         handle.
 * @retval 0 errno is otherwise set to the failing syscall's errno (`lstat`,
 *         `mkdir`, `opendir`, `readdir`, `mkfifo`, `readlink`, `symlink`,
 *         `unlink`, `mknod`, `chmod`, or the underlying `file_copy()` errno).
 */
extern int tree_copy (char const *src, char const *dst) ;

#endif
