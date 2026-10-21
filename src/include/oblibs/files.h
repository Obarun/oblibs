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

/** @Return the size of @filename
 * @Return -1 on failure and set errno*/
extern ssize_t file_get_size(const char* filename) ;

/** Create an empty @name file at @dst directory with mode @mode
 * @Return 1 on success
 * @Return 0 on fail and set errno*/
extern int file_create_empty (char const *dst, char const *name,mode_t mode) ;

/* --- Read/write a whole file, by path ---------------------------------------
 *
 * Thin wrappers over the generic io_readnclose / io_writenclose primitives
 * (io.h): they open @file, transfer, and close. The common open flags are fixed
 * (O_CLOEXEC included). For an uncommon flag set, compose the primitive with
 * io_open() directly, e.g.:
 *   io_writenclose(io_open_mode(f, O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC, 0644), s, n) ;
 *
 * To read a file into a growable buffer instead of a fixed one, use
 * strbuf_read_file() (strbuf.h).
 */

/** Read up to @n bytes of the regular file @file into @buf (O_RDONLY|O_CLOEXEC).
 * @Return the number of bytes read (0..n); a file shorter than @n yields a
 *         partial count, not an error.
 * @Return -1 on error (errno set). */
extern ssize_t file_read(char const *file, char *buf, size_t n) ;

/** Write the @len bytes of @content to @file, creating/truncating it
 * (O_WRONLY|O_TRUNC|O_CREAT|O_CLOEXEC, mode 0666 & ~umask). Not atomic: @file is
 * truncated up front, so a mid-write failure leaves a partial @file.
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int file_write(char const *file, char const *content, size_t len) ;

/** Like file_write but atomic: write @content to a temporary in the same
 * directory then rename() it onto @file, so @file appears all-or-nothing and a
 * failure leaves any previous @file untouched. Equivalent to
 * file_write_atomic_at() with the directory part of @file as temporary
 * directory: the temporary is named "<file>.XXXXXX".
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int file_write_atomic(char const *file, char const *content, size_t len) ;

/** Like file_write_atomic but the temporary is created in @tmpdir instead of the
 * directory of @file. The temporary is named "<tmpdir>/<basename of file>.XXXXXX";
 *
 * @Return 1 on success
 * @Return 0 and set errno on failure; @file is left untouched */
extern int file_write_atomic_at(char const *tmpdir, char const *file, char const *content, size_t len) ;

/** file_read of @dir/@name (composes @dir "/" @name). */
extern ssize_t file_read_at(char const *dir, char const *name, char *buf, size_t n) ;

/** file_write of @dir/@name (composes @dir "/" @name). */
extern int file_write_at(char const *dir, char const *name, char const *content, size_t len) ;

/** Copy the regular-file contents of @src into @dst, creating/truncating @dst
 * with mode @mode. Direct copy, not atomic: @dst is truncated up front, so a
 * mid-copy failure leaves a partial @dst. Native replacement for skalibs
 * filecopy_unsafe.
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int file_copy(char const *src, char const *dst, mode_t mode) ;

/** Fill @n bytes at @x with random [A-Za-z0-9-_] chars (no NUL terminator added).
 * Helper to build a unique temporary name from a template's trailing run of
 * placeholder bytes; the caller owns the retry loop. Native replacement for the
 * name-generation half of skalibs mkfiletemp.
 * @Return 1 on success
 * @Return 0 on fail and set errno (getrandom error) */
extern int file_tmpname(char *x, size_t n) ;

/** Best-effort unlink of @file for cleanup/error paths: removes @file but
 * preserves the caller's errno; an unlink() failure is ignored. Use it to drop
 * a temporary on an error path without clobbering the errno you want to report.
 * Native replacement for skalibs unlink_void.
 * @Return nothing */
extern void file_tryunlink(char const *file) ;
#endif
