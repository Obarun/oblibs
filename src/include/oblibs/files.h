/*
 * files.h
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
 */

#ifndef OB_FILES_H
#define OB_FILES_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#include <oblibs/types.h>

/**
 * @brief Check that a path is one of the file types in a FLAGS_* set.
 *
 * Validates @flags first, then calls `stat(str)` and tests the FLAGS_* bit of
 * its `st_mode` (see mode_to_flags() in types.h) against @flags. The path
 * matches if its type is ANY of the types in @flags. `stat()` follows symbolic
 * links, so the type tested is the one of the link target: `FLAGS_LNK` can
 * never match, and a symbolic link to a directory matches `FLAGS_DIR`. Nothing
 * is logged.
 *
 * @param[in] str   Path to probe. Must not be NULL.
 * @param[in] flags Non-empty OR of FLAGS_* bits (types.h), e.g.
 *                  `FLAGS_REG | FLAGS_DIR`. Any bit outside `FLAGS_ALL` is
 *                  refused, which includes every S_IF* value: an old caller
 *                  passing `S_IFREG` or `S_IFDIR` gets the EINVAL below, before
 *                  `stat()` is called.
 *
 * @return 1 if @str exists and its type is in @flags; the function does not
 *         set `errno` on this path.
 * @return 0 if @flags is invalid or `stat()` fails, `errno` set.
 * @return -1 if @str exists but its type is not in @flags, `errno` set.
 * @retval 0 `errno` is set to `EINVAL` if @flags is 0 or has a bit outside
 *         `FLAGS_ALL`. `stat()` is not called.
 * @retval 0 `errno` is set to the `stat(2)` errno otherwise (e.g. `ENOENT` if
 *         @str does not exist or is a dangling symbolic link, `EACCES` if a
 *         path component is not searchable).
 * @retval -1 `errno` is set to `EINVAL`, the type of @str (after following
 *         symbolic links) is not in @flags. This includes a type
 *         mode_to_flags() does not know (it maps to 0).
 *
 * @note `EINVAL` is returned on two different values: 0 for a bad @flags
 *       argument, -1 for a type mismatch. Tell them apart by the return value,
 *       not by `errno`.
 */
extern ssize_t scan_mode(char const *str, uint32_t flags) ;

/**
 * @brief Get the size of a file, by path.
 *
 * Calls `stat(filename)` and returns its `st_size`. `stat()` follows symbolic
 * links, so the size is the one of the link target. Any file type is accepted;
 * for a non-regular file the value is whatever the kernel reports in `st_size`.
 *
 * @param[in] filename Path of the file. Must not be NULL.
 *
 * @return The size in bytes (>= 0) on success.
 * @return -1 on error, with `errno` set to the `stat(2)` errno (e.g. `ENOENT` if
 *         @filename does not exist, `EACCES` if a path component is not
 *         searchable).
 *
 * @note `errno` is overwritten with 0 before `stat()` is called and is not
 *       restored, so the caller's `errno` is lost on success too.
 * @note `st_size` (`off_t`) is converted to `ssize_t`; the value is truncated on
 *       a build where `ssize_t` is narrower than `off_t`.
 */
extern ssize_t file_get_size(const char* filename) ;

/**
 * @brief Create a new empty file @name inside the absolute directory @dst.
 *
 * Builds the path "@dst/@name" (a '/' is inserted only if @dst does not already
 * end with one), then opens it with `O_WRONLY | O_NONBLOCK | O_CREAT | O_EXCL`
 * and mode @mode, and closes it. The file must not exist yet: `O_EXCL` makes an
 * existing path (including a dangling symbolic link) a failure. The descriptor
 * is not close-on-exec, it is closed before return. The return value of
 * `close()` is not checked. On success `errno` is restored to its entry value.
 *
 * @param[in] dst  Directory to create the file in. Must not be NULL and must be
 *                 an absolute path (start with '/').
 * @param[in] name File name to create. Must not be NULL.
 * @param[in] mode Permission bits passed to `open()`; the effective mode is
 *                 @mode & ~umask.
 *
 * @return 1 on success, the file exists and is empty; `errno` is preserved.
 * @return 0 on failure.
 * @retval 0 `errno` is NOT set, it is left as it was on entry, if @dst does not
 *         start with '/' (this includes an empty @dst). Nothing is created.
 * @retval 0 `errno` is set to the `open(2)` errno otherwise (e.g. `EEXIST` if
 *         the path already exists, `ENOENT` if @dst does not exist, `EACCES`
 *         if @dst is not writable). Nothing is created.
 */
extern int file_create_empty (char const *dst, char const *name,mode_t mode) ;

/*
 * file_read() and file_write() are thin wrappers over the generic
 * io_readnclose() / io_writenclose() primitives (io.h): they open @file,
 * transfer, and close. Their open flags are fixed, `O_CLOEXEC` included. For an
 * uncommon flag set, compose the primitive with io_open() / io_open_mode()
 * directly, e.g.:
 *
 *     io_writenclose(io_open_mode(f, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644), s, n) ;
 *
 * file_read_at() and file_write_at() are the same calls on the path
 * "@dir/@name".
 *
 * file_write_atomic() and file_write_atomic_at() write through a temporary
 * file that is renamed onto the destination, built on the
 * file_tmp_open() / file_tmp_rename() pair declared below. file_copy() copies
 * one path to another.
 *
 * To read a file into a growable buffer instead of a fixed one, use
 * strbuf_read_file() (strbuf.h).
 */

/**
 * @brief Read up to @n bytes of a file into a buffer, by path.
 *
 * Opens @file with `O_RDONLY | O_CLOEXEC` and reads with io_allread() until @n
 * bytes are read, end of file is reached, or an error occurs, then closes the
 * descriptor. No terminating NUL is added. A file larger than @n is not an
 * error: the first @n bytes are read and @n is returned, the caller cannot tell
 * from the result that more data was available. The file type is not checked:
 * any path `open()` accepts read-only is read (a directory fails at `read()`
 * with `EISDIR`).
 *
 * @param[in]  file Path of the file to read. Must not be NULL.
 * @param[out] buf  Destination buffer, at least @n bytes.
 * @param[in]  n    Maximum number of bytes to read. 0 is accepted: the file is
 *                  opened and closed and 0 is returned.
 *
 * @return The number of bytes read, from 0 to @n, on success.
 * @return -1 on error.
 * @retval n The full @n bytes were read; `errno` is not modified by the read.
 * @retval 0..n-1 End of file was reached first (a short file); this is a
 *         success, but `errno` is left set to `EPIPE` (io_allread() reports end
 *         of file that way).
 * @retval -1 `errno` is set to the `open(2)` errno if @file cannot be opened
 *         (e.g. `ENOENT`, `EACCES`).
 * @retval -1 `errno` is set to the `read(2)` errno on a read error (e.g. `EIO`,
 *         `EISDIR`). If a read reports `EAGAIN`/`EWOULDBLOCK`, which requires a
 *         non-blocking file description that this open does not request, -1 is
 *         returned with `errno` 0.
 * @see io_readnclose
 * @see file_read_at
 */
extern ssize_t file_read(char const *file, char *buf, size_t n) ;

/**
 * @brief Write a buffer to a file, by path, creating or truncating it.
 *
 * Opens @file with `O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC` and mode 0666,
 * writes the @len bytes of @content with io_allwrite(), then closes the
 * descriptor. A new file gets mode 0666 & ~umask; an existing file keeps its
 * mode and owner, and a symbolic link is followed. Not atomic: @file is
 * truncated at open time, so a failure during the write leaves a truncated or
 * partially written @file. The return value of `close()` is not checked, so a
 * write error that `close()` alone would report (e.g. on a network
 * filesystem) is not seen. Does not fsync.
 *
 * @param[in] file    Path of the file to write. Must not be NULL.
 * @param[in] content Bytes to write. May be NULL only if @len is 0.
 * @param[in] len     Number of bytes to write. 0 truncates @file to empty (or
 *                    creates it empty) and succeeds.
 *
 * @return 1 on success, all @len bytes were written.
 * @return 0 on failure.
 * @retval 0 `errno` is set to the `open(2)` errno if @file cannot be opened
 *         (e.g. `ENOENT` if the parent directory does not exist, `EACCES`,
 *         `EISDIR`). Nothing was created or truncated.
 * @retval 0 `errno` is set to the `write(2)` errno on a write error (e.g.
 *         `ENOSPC`, `EIO`); @file is left truncated or partially written. A
 *         short write without a `write()` error leaves `errno` 0 or unchanged
 *         (see the module comment).
 * @see io_writenclose
 * @see file_write_at
 * @see file_write_atomic
 */
extern int file_write(char const *file, char const *content, size_t len) ;

/**
 * @brief Write a buffer to a file atomically, with the temporary next to it.
 *
 * Computes the directory part of @file ("." if @file has no '/', "/" if its
 * only '/' is the leading one, otherwise everything before the last '/') and
 * calls file_write_atomic_at() with it as temporary directory. The temporary is
 * therefore created in the same directory as @file, which guarantees that the
 * final `rename()` does not cross filesystems; its path designates
 * "<file>.XXXXXX" (written "./<file>.XXXXXX" when @file has no '/'), the six
 * 'X' being replaced by `mkstemp()`. Every behaviour, side effect and failure path is
 * the one of file_write_atomic_at(), except that its `EINVAL` path for an
 * empty temporary directory cannot be reached from here.
 *
 * @param[in] file    Path of the destination file. Must not be NULL.
 * @param[in] content Bytes to write. May be NULL only if @len is 0.
 * @param[in] len     Number of bytes to write.
 *
 * @return 1 on success, @file now holds exactly @content.
 * @return 0 on failure, with `errno` set as described for
 *         file_write_atomic_at() (a `mkstemp`, `fcntl`, `fchmod`, `write`,
 *         `close` or `rename` errno; a short write without a `write()` error
 *         leaves `errno` 0 or unchanged). @file is left untouched.
 * @see file_write_atomic_at
 */
extern int file_write_atomic(char const *file, char const *content, size_t len) ;

/**
 * @brief Write a buffer to a file atomically, with the temporary in @tmpdir.
 *
 * Creates the temporary "<tmpdir>/<basename of file>.XXXXXX" with
 * file_tmp_open() (no '/' is added if @tmpdir already ends with one; the
 * basename is the part of @file after its last '/', or all of @file), writes
 * the @len bytes of @content to it with io_allwrite(), then closes it and
 * `rename()`s it onto @file with file_tmp_rename(). On success @file is
 * replaced in one step: a reader sees either the previous content or the new
 * one, never a mix.
 *
 * Side effects and properties:
 *   - the result has mode 0644 exactly, set by `fchmod()`, so it is not subject
 *     to the umask; it is owned by the caller (no `fchown()` is done). The
 *     mode, owner and hard links of a previous @file are not preserved, since
 *     @file becomes a new inode; if @file is a symbolic link, the link itself
 *     is replaced, not its target.
 *   - no fsync of the file nor of its directory: the new content is not
 *     guaranteed to be on stable storage when the function returns. Use
 *     file_tmp_open() / sync_fd() / file_tmp_rename() directly when durability
 *     is needed.
 *   - @tmpdir must be on the same filesystem as @file, otherwise the final
 *     `rename()` fails with `EXDEV`.
 *   - on every failure after the temporary was created, it is removed with
 *     file_tryunlink() (best effort, an `unlink()` failure is ignored and
 *     `errno` is preserved); its descriptor is always closed.
 *
 * @param[in] tmpdir  Directory to create the temporary in. Must not be NULL;
 *                    must not be empty.
 * @param[in] file    Path of the destination file. Must not be NULL.
 * @param[in] content Bytes to write. May be NULL only if @len is 0.
 * @param[in] len     Number of bytes to write. 0 produces an empty @file.
 *
 * @return 1 on success, @file now holds exactly @content.
 * @return 0 on failure; in every case @file is left untouched.
 * @retval 0 `errno` is set to `EINVAL` if @tmpdir is the empty string. Nothing
 *         is created.
 * @retval 0 `errno` is set by file_tmp_open() if the temporary cannot be set up:
 *         the `mkstemp(3)` errno (nothing created), or the `fcntl(2)` errno of
 *         cloexec_fd() or the `fchmod(2)` errno (temporary removed).
 * @retval 0 `errno` is set to the `write(2)` errno if writing the content fails
 *         (e.g. `ENOSPC`); the temporary is removed. A short write without a
 *         `write()` error leaves `errno` 0 or unchanged (see the module
 *         comment).
 * @retval 0 `errno` is set by file_tmp_rename(): the `close(2)` errno or the
 *         `rename(2)` errno (e.g. `EXDEV`, `EISDIR`); the temporary is removed.
 * @see file_write_atomic
 * @see file_tmp_open
 * @see file_tmp_rename
 */
extern int file_write_atomic_at(char const *tmpdir, char const *file, char const *content, size_t len) ;

/*
 * file_tmp_open() and file_tmp_rename() are the two halves of an atomic
 * write, for a caller that needs more than file_write_atomic_at() offers (an
 * owner, a mode, several writes, an fsync). Composition:
 *
 *     char tmp[] = "/dir/file.XXXXXX" ;
 *     int fd = file_tmp_open(tmp, 0644, (uid_t)-1, (gid_t)-1) ;
 *     if (fd < 0)
 *         return 0 ;
 *     if (io_allwrite(fd, s, len) < len || sync_fd(fd) < 0) {
 *         close_fd(fd) ;
 *         file_tryunlink(tmp) ;
 *         return 0 ;
 *     }
 *     return file_tmp_rename(fd, tmp, "/dir/file") ;
 *
 * sync_fd() is only needed when durability is required.
 */

/**
 * @brief Create and open a temporary file for an atomic write.
 *
 * Creates the file with `mkstemp(tmpl)`, which opens it with
 * `O_RDWR | O_CREAT | O_EXCL` and mode 0600, then:
 *   1. marks the descriptor close-on-exec with cloexec_fd() (fd.h). The
 *      descriptor is NOT close-on-exec between `mkstemp()` and this call: a
 *      `fork()` + `exec()` run concurrently by another thread in that window
 *      inherits it.
 *   2. only if @uid or @gid is not -1, calls `fchown(fd, uid, gid)`; a -1 @uid
 *      or @gid leaves that owner unchanged.
 *   3. calls `fchmod(fd, mode)`, which sets @mode exactly, not subject to the
 *      umask. `fchown()` runs before `fchmod()` because a `chown` clears
 *      `S_ISUID`/`S_ISGID`; applying @mode last lets those bits be set when
 *      @mode asks for them (within the kernel's own rules, e.g. Linux silently
 *      drops `S_ISGID` for an unprivileged caller who is not a member of the
 *      file's group).
 *
 * Place @tmpl on the same filesystem as the final destination: `rename()` does
 * not cross filesystems, file_tmp_rename() would fail with `EXDEV`.
 *
 * @param[in,out] tmpl Template path, chosen by the caller. Must not be NULL,
 *                     must be writable and must end with "XXXXXX". Modified in
 *                     place by `mkstemp()`: on success it holds the path of the
 *                     created temporary (needed by file_tmp_rename()).
 * @param[in]     mode Exact permission bits of the temporary.
 * @param[in]     uid  Owner to set, or `(uid_t)-1` to keep the caller's.
 * @param[in]     gid  Group to set, or `(gid_t)-1` to keep the default one.
 *
 * @return The descriptor (>= 0), open read-write and close-on-exec, on success.
 *         The caller owns it and must pass it to file_tmp_rename(), or close it
 *         and remove @tmpl.
 * @return -1 on failure.
 * @retval -1 `errno` is set to the `mkstemp(3)` errno (e.g. `EINVAL` if @tmpl
 *         does not end with "XXXXXX", `EEXIST` if no unique name could be
 *         made, or an `open(2)` errno such as `EACCES`/`ENOENT` for the
 *         directory). Nothing was created.
 * @retval -1 `errno` is set to the `fcntl(2)` errno of cloexec_fd(), the
 *         `fchown(2)` errno (e.g. `EPERM`) or the `fchmod(2)` errno. The
 *         descriptor is closed and the temporary is removed with
 *         file_tryunlink() (best effort, `errno` is preserved).
 * @see file_tmp_rename
 */
extern int file_tmp_open(char *tmpl, mode_t mode, uid_t uid, gid_t gid) ;

/**
 * @brief Create and open a temporary file in a given directory.
 *
 * Composes in @tmp the template "<dir>/XXXXXX" (no '/' is added if @dir
 * already ends with one, the same rule as file_write_atomic_at()), then
 * delegates to file_tmp_open(@tmp, @mode, @uid, @gid) and returns its result
 * unchanged. Move the temporary onto its destination with file_tmp_rename().
 *
 * @dir must be on the same filesystem as the final destination: `rename()`
 * does not cross filesystems, file_tmp_rename() would fail with `EXDEV`.
 *
 * @param[out] tmp    Buffer receiving the template, then the path of the
 *                    created temporary on success (needed by file_tmp_rename()).
 *                    Must not be NULL.
 * @param[in]  tmplen Size of @tmp in bytes, trailing NUL included.
 * @param[in]  dir    Directory of the temporary. Must not be NULL.
 * @param[in]  mode   Exact permission bits of the temporary, see file_tmp_open().
 * @param[in]  uid    Owner to set, or `(uid_t)-1` to keep the caller's.
 * @param[in]  gid    Group to set, or `(gid_t)-1` to keep the default one.
 *
 * @return The descriptor (>= 0), open read-write and close-on-exec, on success,
 *         @tmp then holds the path of the temporary.
 * @return -1 on failure.
 * @retval -1 `errno` is set to `EINVAL` if @dir is empty. Nothing was created,
 *         @tmp is not modified.
 * @retval -1 `errno` is set to `ENAMETOOLONG` if the template, trailing NUL
 *         included, does not fit in @tmplen bytes. Nothing was created, @tmp is
 *         not modified and is not a valid path.
 * @retval -1 `errno` is set by file_tmp_open(), with its cleanup: the
 *         `mkstemp(3)` errno (e.g. `ENOENT` if @dir does not exist), nothing
 *         created, or the cloexec_fd()/`fchown(2)`/`fchmod(2)` errno, the
 *         temporary removed.
 * @see file_tmp_open
 * @see file_tmp_rename
 */
extern int file_tmp_open_at(char *tmp, size_t tmplen, char const *dir, mode_t mode, uid_t uid, gid_t gid) ;

/**
 * @brief Close a temporary's descriptor and rename the temporary onto its destination.
 *
 * Calls `close(fd)` and checks it; only if it succeeds, calls
 * `rename(tmp, dst)`, which replaces @dst in one step. @fd is always consumed:
 * it is closed first whatever happens next, and `close()` is not retried on
 * `EINTR` because Linux releases the descriptor even when `close()` fails;
 * never use @fd again after the call. Does not fsync, neither the file nor the
 * parent directory: call sync_fd() (fd.h) on @fd before when durability is
 * needed.
 *
 * @param[in] fd  Descriptor of the temporary, as returned by file_tmp_open().
 *                Consumed.
 * @param[in] tmp Path of the temporary. Must not be NULL.
 * @param[in] dst Destination path. Must not be NULL; must be on the same
 *                filesystem as @tmp.
 *
 * @return 1 on success, @dst now holds the content of @tmp.
 * @return 0 on failure; in both cases below @tmp is removed with
 *         file_tryunlink() (best effort, `errno` is preserved) and @dst is left
 *         untouched.
 * @retval 0 `errno` is set to the `close(2)` errno (e.g. `EIO`, `EBADF` for an
 *         invalid @fd); `rename()` was not attempted.
 * @retval 0 `errno` is set to the `rename(2)` errno (e.g. `EXDEV` across
 *         filesystems, `EISDIR` if @dst is a directory).
 * @see file_tmp_open
 */
extern int file_tmp_rename(int fd, char const *tmp, char const *dst) ;

/**
 * @brief Read up to @n bytes of the file "@dir/@name" into a buffer.
 *
 * Builds the path @dir "/" @name in a stack buffer (a '/' is always inserted,
 * even if @dir already ends with one) and calls file_read() on it. Every
 * behaviour, return value and errno is the one of file_read().
 *
 * @param[in]  dir  Directory part. Must not be NULL.
 * @param[in]  name File name part. Must not be NULL.
 * @param[out] buf  Destination buffer, at least @n bytes.
 * @param[in]  n    Maximum number of bytes to read.
 *
 * @return As file_read(): the number of bytes read (0..n) on success, -1 on
 *         error with `errno` set to the `open(2)` or `read(2)` errno. A short
 *         read (end of file before @n) succeeds and leaves `errno` set to
 *         `EPIPE`.
 * @see file_read
 */
extern ssize_t file_read_at(char const *dir, char const *name, char *buf, size_t n) ;

/**
 * @brief Write a buffer to the file "@dir/@name", creating or truncating it.
 *
 * Builds the path @dir "/" @name in a stack buffer (a '/' is always inserted,
 * even if @dir already ends with one) and calls file_write() on it. Every
 * behaviour, side effect, return value and errno is the one of file_write():
 * not atomic, mode 0666 & ~umask for a new file, no fsync.
 *
 * @param[in] dir     Directory part. Must not be NULL.
 * @param[in] name    File name part. Must not be NULL.
 * @param[in] content Bytes to write. May be NULL only if @len is 0.
 * @param[in] len     Number of bytes to write.
 *
 * @return As file_write(): 1 on success, 0 on failure with `errno` set to the
 *         `open(2)` or `write(2)` errno (a short write without a `write()`
 *         error leaves `errno` 0 or unchanged).
 * @see file_write
 */
extern int file_write_at(char const *dir, char const *name, char const *content, size_t len) ;

/**
 * @brief Copy the contents of a file to another path, non-atomically.
 *
 * Opens @src with `O_RDONLY`, opens @dst with `O_WRONLY | O_CREAT | O_TRUNC`
 * and mode @mode, then copies with `read()` / io_allwrite() through a 64 KiB
 * stack buffer until end of file, and closes both descriptors. Direct copy, not
 * atomic: @dst is truncated at open time, so a failure during the copy leaves a
 * truncated or partial @dst in place (it is not removed). A new @dst gets mode
 * @mode & ~umask; an existing @dst keeps its mode and owner. Neither descriptor
 * is opened with `O_CLOEXEC` (both are closed before return). The file type of
 * @src is not checked. The return value of `close()` is not checked. Does not
 * fsync. Native replacement for skalibs `filecopy_unsafe`.
 *
 * @param[in] src  Path of the file to copy. Must not be NULL.
 * @param[in] dst  Path of the copy. Must not be NULL. Must not name the same
 *                 file as @src: @dst is truncated before @src is read, so the
 *                 result would be an empty file.
 * @param[in] mode Permission bits used if @dst is created.
 *
 * @return 1 on success, @dst holds the content of @src.
 * @return 0 on failure.
 * @retval 0 `errno` is set to the `open(2)` errno of @src. Nothing was created.
 * @retval 0 `errno` is set to the `open(2)` errno of @dst; @src is closed.
 *         Nothing was created or truncated.
 * @retval 0 `errno` is set to the `read(2)` errno of @src (e.g. `EIO`,
 *         `EISDIR`); @dst is left partial.
 * @retval 0 `errno` is set to the `write(2)` errno of @dst (e.g. `ENOSPC`); @dst
 *         is left partial. A short write without a `write()` error leaves
 *         `errno` 0 or unchanged (see the module comment).
 */
extern int file_copy(char const *src, char const *dst, mode_t mode) ;

/**
 * @brief Fill a buffer with random file-name characters.
 *
 * Draws @n random bytes with `getrandom(2)` (flags 0, so it blocks until the
 * kernel entropy pool is initialized) and maps each one to a character of
 * "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_" (64
 * characters, so the mapping is uniform), written to @x. No terminating NUL is
 * added. Meant to fill the trailing placeholder run of a temporary-name
 * template; the caller owns the create/retry loop. Native replacement for the
 * name-generation half of skalibs `mkfiletemp`.
 *
 * @param[out] x Destination, at least @n bytes. Must not be NULL.
 * @param[in]  n Number of characters to generate. Must be > 0: the random bytes
 *               are drawn into a variable-length array of @n bytes on the stack,
 *               and a zero-length one is undefined behaviour in C.
 *
 * @return 1 on success, @x[0..n-1] holds @n random characters.
 * @return 0 on failure; @x is not modified.
 * @retval 0 `errno` is set to the `getrandom(2)` errno if it fails (e.g.
 *         `EINTR` if a signal interrupts it while it blocks, `ENOSYS`).
 * @retval 0 `errno` is NOT set, it is left unchanged, if `getrandom()` returns
 *         fewer than @n bytes (possible for a request above 256 bytes
 *         interrupted by a signal).
 */
extern int file_tmpname(char *x, size_t n) ;

/**
 * @brief Remove a file, ignoring failure and preserving `errno`.
 *
 * Calls `unlink(file)`, ignores its result, and restores `errno` to its entry
 * value. Use it to drop a temporary on an error path without clobbering the
 * errno you want to report. Native replacement for skalibs `unlink_void`.
 *
 * @param[in] file Path to remove. Must not be NULL.
 *
 * @return Nothing. `errno` is left exactly as it was on entry, whether
 *         `unlink()` succeeded or not; the caller cannot tell whether @file was
 *         removed.
 */
extern void file_tryunlink(char const *file) ;
#endif
