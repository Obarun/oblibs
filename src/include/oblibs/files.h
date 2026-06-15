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
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_FILES_H
#define OB_FILES_H

#include <sys/types.h>
#include <stddef.h>

/** Scan if @str has the corresponding @mode
 * @Return 1 on success
 * @Return 0 if @str doesn't exist or stat fail
 * @Return -1 if @str haven't the corresponding @mode
 * Also warm the user if stat fail*/
extern ssize_t scan_mode(char const *str, mode_t mode) ;

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
 * failure leaves any previous @file untouched.
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int file_write_atomic(char const *file, char const *content, size_t len) ;

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
