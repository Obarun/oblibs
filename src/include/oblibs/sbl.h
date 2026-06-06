/*
 * sbl.h
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
 *
 * sbl -- "strbuf list": a list of NUL-separated strings stored in a strbuf.
 *
 * This module is the oblibs-native replacement that merges the old `stack`
 * (src/stack) and `sastr` (src/sastr) modules into a single API built on top of
 * `strbuf`. It carries no skalibs dependency, and inherits strbuf's memory
 * behaviour: the buffer grows geometrically and migrates from a borrowed
 * (stack) seed to the heap on overflow. There is no hard capacity bound.
 *
 * Representation:
 *  - An sbl is just a `strbuf` whose content is a sequence of C strings, each
 *    terminated by its NUL byte. `len` INCLUDES the NUL of every element,
 *    including the last one. An empty list has len == 0.
 *  - sbl_add(sb, "abc") therefore appends 4 bytes ("abc\0"). The buffer is
 *    always a valid C string covering [0, len).
 *  - There is no "close" operation (unlike stack): every sbl_* function leaves
 *    the buffer in this canonical, NUL-terminated state.
 *  - Iterate with FOREACH_SBL. The element count is computed on demand with
 *    sbl_count(); it is never cached in the struct.
 *
 * Because an sbl IS a strbuf, raw strbuf_* primitives (strbuf_catb,
 * strbuf_terminate, ...) may be mixed in when building a list by hand: append
 * the element bytes, then strbuf_terminate() to close the element with its NUL.
 */

#ifndef OB_SBL_H
#define OB_SBL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/strbuf.h>

/** Iterate over each element of an sbl. @pos is a size_t cursor (byte offset of
 *  the current element); the body sees the element at sb->s + pos. */
#define FOREACH_SBL(sb, pos) \
    strbuf_ref __sbl_ref__ = (sb) ; \
    for (; (size_t)(pos) < __sbl_ref__->len ; (pos) += strlen(__sbl_ref__->s + (size_t)(pos)) + 1)

/** Scope-local sbl seeded with @n bytes of on-stack storage (see _alloc_strbuf_). */
#define _alloc_sbl_(sb, n) _alloc_strbuf_(sb, n)
#define _cleanup_sbl_ _cleanup_strbuf_
#define SBL_ZERO STRBUF_ZERO

/* ---- building / mutating --------------------------------------------------*/

/** @brief Append @n bytes from @s as a new element, then its terminating NUL.
 *  Copies exactly @n bytes: @s need NOT be NUL-terminated (no strlen, no
 *  over-read) -- use this for a slice of a larger buffer. @n must be > 0.
 *  @return 1 on success, 0 on failure (errno: EINVAL on @n == 0, ERANGE on
 *  allocation failure). */
extern int sbl_addb(strbuf *sb, char const *s, size_t n) ;

/** @brief Append the C string @s as a new element (its NUL is stored). The
 *  caller vouches that @s is NUL-terminated. @s must be non-empty. */
#define sbl_add(sb, s) sbl_addb(sb, (s), strlen(s))

/** @brief Insert @s (without its NUL) at byte @offset (0..len). The caller is
 *  responsible for offset landing on an element boundary. @return 1 / 0 (errno). */
extern int sbl_insert(strbuf *sb, size_t offset, char const *s) ;

/** @brief Remove the element starting at byte @offset (as returned by
 *  sbl_search / sbl_element_byid). @return 1 / 0 (errno). */
extern int sbl_remove_at(strbuf *sb, size_t offset) ;

/** @brief Remove the first element equal to @s, if any (no-op if absent).
 *  @return 1 / 0 (errno). */
extern int sbl_remove(strbuf *sb, char const *s) ;

/** @brief Reverse the order of the elements. @return 1 / 0 (errno). */
extern int sbl_reverse(strbuf *sb) ;

/** @brief Sort the elements lexicographically (ascending). @return 1 / 0. */
extern int sbl_sort(strbuf *sb) ;

/** @brief Drop consecutive duplicate elements. Assumes @sb is already sorted.
 *  @return 1 / 0. */
extern int sbl_drop_duplicate(strbuf *sb) ;

/** @brief Convenience: sbl_sort() then sbl_drop_duplicate(). @return 1 / 0. */
extern int sbl_sortndrop(strbuf *sb) ;

/** @brief Drop any element that is a subtree of a previous one (e.g. drop
 *  "/foo/bar" when "/foo" is present). Assumes @sb is already sorted.
 *  @return 1 / 0. */
extern int sbl_dir_drop_child(strbuf *sb) ;

/* ---- rebuilding into a single string --------------------------------------*/

/** @brief Replace the list by its elements joined with a single space, with no
 *  trailing separator. @return 1 / 0. */
extern int sbl_rebuild_oneline(strbuf *sb) ;

/** @brief Replace the list by its elements joined with '\n', KEEPING a trailing
 *  '\n'. @return 1 / 0. */
extern int sbl_rebuild_nline(strbuf *sb) ;

/** @brief Replace the list by its elements joined with @delim, with no trailing
 *  separator. @return 1 / 0. */
extern int sbl_rebuild_with_delim(strbuf *sb, unsigned int delim) ;

/* ---- querying -------------------------------------------------------------*/

/** @brief Number of elements. */
extern size_t sbl_count(strbuf *sb) ;

/** @brief Byte offset of the first element exactly equal to @s, or -1. */
extern ssize_t sbl_search(strbuf *sb, char const *s) ;

/** @brief Byte offset of the first occurrence of substring @str anywhere in the
 *  buffer, or -1. */
extern ssize_t sbl_find(strbuf *sb, char const *str) ;

/** @brief Byte offset of the element at index @id (0-based), or -1. */
extern ssize_t sbl_element_byid(strbuf *sb, unsigned int id) ;

/** @brief Copy the whole NUL-separated content (len bytes) into @dst, which must
 *  be large enough. */
extern void sbl_to_char(char *dst, strbuf *sb) ;

/* ---- directory helpers ----------------------------------------------------*/

/** @brief Append to @sb the names in @srcdir matching @mode, excluding the NULL-
 *  terminated names in @exclude. @return 1 / 0. */
extern int sbl_dir_get(strbuf *sb, char const *srcdir, char const **exclude, mode_t mode) ;

/** @brief Recursive sbl_dir_get using lstat(); prefixes @srcdir/ when
 *  @include_path is set. @return 1 / 0. */
extern int sbl_dir_get_recursive(strbuf *sb, char const *srcdir, char const **exclude, mode_t mode, uint8_t include_path) ;

/** @brief Append occurrences of @element found in @srcdir matching @mode,
 *  prefixed by @srcdir/ when @include_path is set. @return number found, or 0
 *  on error (errno set). */
extern int sbl_dir_get_element(strbuf *sb, char const *srcdir, char const *element, mode_t mode, uint8_t include_path) ;

/* ---- lexer-backed list building (built above the lexer) -------------------*/

/** @brief Tokenise @string (escaping ' \t\r\n', splitting on spaces/newlines)
 *  into @sb, replacing its content. @string must not alias @sb->s.
 *  @return 1 / 0 (errno: EINVAL on empty @string). */
extern int sbl_clean_string(strbuf *sb, char const *string) ;

/** @brief Read @sb->s as one C string and split it into one element per line.
 *  @return 1 / 0. */
extern int sbl_split_string_in_nline(strbuf *sb) ;

/** @brief Replace every occurrence of the exact string @regex with @by, across
 *  all elements, rebuilding @sb. @return 1 / 0 (errno: EINVAL on bad args). */
extern int sbl_replace(strbuf *sb, char const *regex, char const *by) ;

/** @brief Split @sb into lines, sbl_replace on each, then rejoin with '\n'.
 *  @return 1 / 0. */
extern int sbl_replace_g(strbuf *sb, char const *regex, char const *by) ;

#endif
