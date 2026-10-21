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
 * except according to the terms contained in the LICENSE file.
 *
 * @brief "strbuf list" -- a list of NUL-separated strings stored in a strbuf.
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
 *    including the last one. An empty list has `len == 0`.
 *  - `sbl_add(sb, "abc")` therefore appends 4 bytes (`"abc\0"`). The buffer is
 *    always a valid C string covering `[0, len)`.
 *  - There is no "close" operation (unlike stack): every `sbl_*` function leaves
 *    the buffer in this canonical, NUL-terminated state.
 *  - Zero/NUL policy: an element's NUL is part of the content (counted in `len`).
 *    `sbl_add`/`sbl_addb` WRITE that NUL (they append a whole element), so
 *    chaining them just works (the next append lands after the previous NUL).
 *    `sbl_insert` does NOT write a NUL: it prefixes an existing element, which
 *    supplies the closing NUL -- hence its offset must start an existing element
 *    (`< len`).
 *  - Iterate with `FOREACH_SBL`. The element count is computed on demand with
 *    `sbl_count()`; it is never cached in the struct.
 *
 * Because an sbl IS a strbuf, raw `strbuf_*` primitives (`strbuf_catb`,
 * `strbuf_terminate`, ...) may be mixed in when building a list by hand: append
 * the element bytes, then `strbuf_terminate()` to close the element with its NUL.
 *
 * @note Errno on allocation failure: every mutating function in this module that
 *       grows the buffer forwards `strbuf`'s failure behaviour. A failed append
 *       sets `errno` to `ERANGE` when the requested size would overflow `size_t`,
 *       and otherwise leaves `errno` as set by the allocator (`malloc`/`realloc`,
 *       typically `ENOMEM`) -- `strbuf_reserve()` does NOT normalise the
 *       allocator's errno. Where this document says "the strbuf failure errno",
 *       it means exactly that pair of cases.
 */

#ifndef OB_SBL_H
#define OB_SBL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/strbuf.h>
#include <oblibs/types.h>

/**
 * @brief Iterate over each element of an sbl.
 *
 * Walks the NUL-separated elements of @sb in storage order. @pos is a `size_t`
 * cursor holding the byte offset of the current element; on each iteration the
 * body sees the element as the C string `sb->s + pos`. The macro declares a
 * `strbuf_ref __sbl_ref__` in the enclosing scope, so it may appear at most once
 * per scope. @pos must be a writable lvalue initialised by the caller (normally
 * to 0) and is advanced past each element's NUL.
 *
 * @param[in] sb   The sbl to iterate (a `strbuf *`).
 * @param[in,out] pos `size_t` cursor; set to 0 to start, holds the current
 *                    element's byte offset in the body.
 */
#define FOREACH_SBL(sb, pos) \
    strbuf_ref __sbl_ref__ = (sb) ; \
    for (; (size_t)(pos) < __sbl_ref__->len ; (pos) += strlen(__sbl_ref__->s + (size_t)(pos)) + 1)

/**
 * @brief Declare a scope-local sbl seeded with @n bytes of on-C-stack storage.
 *
 * Thin alias of `_alloc_strbuf_`: an sbl is a strbuf, so it is seeded and freed
 * exactly the same way. The seed lives on the current stack frame; see the
 * `_alloc_strbuf_` note in strbuf.h for the recursion caveat.
 *
 * @param[out] sb Identifier of the strbuf to declare.
 * @param[in] n   Requested seed size in bytes.
 * @see _alloc_strbuf_
 */
#define _alloc_sbl_(sb, n) _alloc_strbuf_(sb, n)

/** @brief Automatic `strbuf_free()` at scope exit; alias of `_cleanup_strbuf_`. */
#define _cleanup_sbl_ _cleanup_strbuf_

/** @brief Zero-initialiser for an sbl; alias of `STRBUF_ZERO` (empty list). */
#define SBL_ZERO STRBUF_ZERO

/**
 * @brief Append @n bytes from @s as a new element, then its terminating NUL.
 *
 * Copies exactly @n bytes from @s (no `strlen`, no over-read), then writes the
 * element's terminating NUL via `strbuf_terminate()`. Because the count is
 * explicit, @s need NOT be NUL-terminated -- use this for a slice of a larger
 * buffer. The list stays canonical: `len` grows by `n + 1`.
 *
 * @param[in,out] sb Destination sbl; the element is appended at `sb->len`.
 * @param[in] s   Source bytes; the first @n are read. Not required to be
 *                NUL-terminated. Must point to at least @n readable bytes.
 * @param[in] n   Number of bytes to copy; must be > 0.
 * @return 1 on success.
 * @return 0 on failure:
 *      - `errno` is set to `EINVAL` if @n == 0 (nothing is appended).
 *      - otherwise the append failed; `errno` is the strbuf failure errno
 *        (`ERANGE` on size overflow, else the allocator errno).
 * @note On the strbuf-failure path the element bytes may already have been
 *       copied before the terminating NUL failed; @sb is left with `len`
 *       advanced by the partial write. Callers that need atomicity must snapshot
 *       `sb->len` and restore it themselves.
 */
extern int sbl_addb(strbuf *sb, char const *s, size_t n) ;

/**
 * @brief Append the C string @s as a new element (its NUL is stored).
 *
 * Convenience wrapper over `sbl_addb()` that computes the length with `strlen`.
 * The caller vouches that @s is a valid NUL-terminated C string. Because
 * `sbl_addb` rejects a zero length, @s must be non-empty.
 *
 * @param[in,out] sb Destination sbl.
 * @param[in] s   NUL-terminated, non-empty C string to append.
 * @return Same contract as `sbl_addb()`: 1 on success; 0 on failure with
 *         `errno` set to `EINVAL` when @s is empty, else the strbuf failure errno.
 * @see sbl_addb
 */
#define sbl_add(sb, s) sbl_addb(sb, (s), strlen(s))

/**
 * @brief Prefix the existing element starting at byte @offset with @s's bytes.
 *
 * Inserts the bytes of @s (its `strlen`, no NUL) immediately before the element
 * whose first byte is at @offset, via `strbuf_insertb()`. No NUL is written: the
 * prefixed element keeps its own closing NUL, so the list stays canonical.
 * @offset MUST be the start of an EXISTING element (`0 <= offset < len`, e.g. a
 * value returned by `sbl_search` / `sbl_element_byid`). `offset == len` and the
 * empty list are rejected, because there would be no element NUL to terminate
 * the inserted bytes -- use `sbl_add` to append a new element instead.
 *
 * @param[in,out] sb Target sbl; must contain at least one element.
 * @param[in] offset Byte offset of an existing element; must satisfy
 *                   `0 <= offset < sb->len`.
 * @param[in] s   NUL-terminated, non-empty C string to insert as a prefix.
 * @return 1 on success.
 * @return 0 on failure:
 *      - `errno` is set to `EINVAL` if @s is empty (`*s == 0`) or @offset >=
 *        `sb->len` (this also covers the empty list, where `len == 0`).
 *      - otherwise the insert failed; `errno` is the strbuf failure errno
 *        (`ERANGE` on size overflow, else the allocator errno).
 */
extern int sbl_insert(strbuf *sb, size_t offset, char const *s) ;

/**
 * @brief Remove the element starting at byte @offset.
 *
 * Deletes the single element whose first byte is at @offset (as returned by
 * `sbl_search` / `sbl_element_byid`), shifting the remaining bytes down and
 * shrinking `len` by the element's length plus its NUL. The operation is purely
 * in-place: no allocation, so it cannot fail on memory.
 *
 * @param[in,out] sb Target sbl; must contain the element at @offset.
 * @param[in] offset Byte offset of the element to remove; must satisfy
 *                   `0 <= offset < sb->len`.
 * @return 1 on success.
 * @return 0 on failure; `errno` is set to `EINVAL` if the list is empty
 *         (`sb->len == 0`) or @offset >= `sb->len`.
 * @note @offset is taken on faith to start an element. A value that points into
 *       the middle of an element is not detected; it removes from that byte to
 *       the next NUL, corrupting the list. Only pass offsets obtained from
 *       `sbl_search`, `sbl_element_byid` or `FOREACH_SBL`.
 */
extern int sbl_remove_at(strbuf *sb, size_t offset) ;

/**
 * @brief Remove the first element exactly equal to @s, if present.
 *
 * Looks @s up with `sbl_search()` and, when found, deletes it with
 * `sbl_remove_at()`. If @s is absent the list is left unchanged and success is
 * still reported (this is a no-op, not an error).
 *
 * @param[in,out] sb Target sbl.
 * @param[in] s   NUL-terminated C string to match exactly.
 * @return 1 on success, INCLUDING the case where @s was not found (no-op).
 * @return 0 only if the underlying `sbl_remove_at()` fails for the located
 *         element; `errno` is then `EINVAL` (see `sbl_remove_at`). This path is
 *         unreachable for an offset just returned by `sbl_search`.
 */
extern int sbl_remove(strbuf *sb, char const *s) ;

/**
 * @brief Reverse the order of the elements.
 *
 * Rebuilds @sb with its elements in reverse order, via a scratch buffer, then
 * copies the result back. Element bytes (including each NUL) are preserved
 * exactly; only their order changes. An empty list reverses to an empty list and
 * succeeds.
 *
 * @param[in,out] sb Target sbl.
 * @return 1 on success.
 * @return 0 on failure; `errno` is the strbuf failure errno (`ERANGE` on size
 *         overflow, else the allocator errno) from building or copying the
 *         scratch buffer.
 */
extern int sbl_reverse(strbuf *sb) ;

/**
 * @brief Sort the elements in ascending order according to @cmp.
 *
 * Builds a VLA of pointers into `sb->s` (one per element), selection-sorts those
 * pointers with @cmp (ascending), then rebuilds @sb from the sorted pointers via
 * a scratch buffer copied back over @sb. There is no per-element length limit:
 * elements may be of any size. This is the general form of `sbl_sort`, which is
 * just this function called with `strcmp`.
 *
 * @param[in,out] sb Target sbl.
 * @param[in] cmp Comparator following the `strcmp` convention: given two element
 *                C strings @a and @b, it returns < 0 if @a must precede @b, 0 if
 *                they are equivalent, and > 0 if @a must follow @b. Equivalent
 *                elements keep their relative order (the sort uses `> 0`, i.e. it
 *                is a strict, non-reordering comparison for ties).
 * @return 1 on success.
 * @return 0 in two distinct cases:
 *      - the list is empty (`sb->len == 0`); `errno` is NOT set. An empty list
 *        is reported as 0, not 1.
 *      - building or copying back the scratch buffer failed; `errno` is the
 *        strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno).
 * @note The pointer table is a VLA sized by the element count, so a list with a
 *       very large number of elements can exhaust the C stack.
 */
extern int sbl_sort_cmp(strbuf *sb, int (*cmp)(char const *a, char const *b)) ;

/**
 * @brief Sort the elements lexicographically in ascending order.
 *
 * Thin wrapper over `sbl_sort_cmp` called with `strcmp`, sorting the elements by
 * ascending byte order. See `sbl_sort_cmp` for the full mechanics (VLA of
 * pointers, scratch rebuild) and the shared return/errno contract.
 *
 * @param[in,out] sb Target sbl.
 * @return 1 on success.
 * @return 0 in two distinct cases:
 *      - the list is empty (`sb->len == 0`); `errno` is NOT set. An empty list
 *        is reported as 0, not 1.
 *      - building or copying back the scratch buffer failed; `errno` is the
 *        strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno).
 * @note The pointer table is a VLA sized by the element count, so a list with a
 *       very large number of elements can exhaust the C stack.
 * @see sbl_sort_cmp
 */
extern int sbl_sort(strbuf *sb) ;

/**
 * @brief Drop consecutive duplicate elements.
 *
 * Walks @sb once and keeps an element only when it differs (by `strcmp`) from
 * the element kept immediately before it, rebuilding @sb through a scratch
 * buffer. Only ADJACENT duplicates are removed, so @sb must already be sorted
 * for this to remove all duplicates. An empty list succeeds unchanged.
 *
 * @param[in,out] sb Target sbl; assumed already sorted.
 * @return 1 on success.
 * @return 0 on failure; `errno` is the strbuf failure errno (`ERANGE` on size
 *         overflow, else the allocator errno) from building or copying the
 *         scratch buffer.
 */
extern int sbl_drop_duplicate(strbuf *sb) ;

/**
 * @brief Sort the elements, then drop duplicates.
 *
 * Convenience composition of `sbl_sort()` followed by `sbl_drop_duplicate()`.
 *
 * @param[in,out] sb Target sbl.
 * @return 1 on success.
 * @return 0 if either step fails. `errno` follows the failing step: for the
 *         `sbl_sort()` step it is unset when @sb was empty, otherwise the strbuf
 *         failure errno; for the `sbl_drop_duplicate()` step, the strbuf failure
 *         errno.
 * @note Because `sbl_sort()` reports an empty list as failure (0, no errno),
 *       `sbl_sortndrop()` likewise returns 0 on an empty @sb.
 * @see sbl_sort
 * @see sbl_drop_duplicate
 */
extern int sbl_sortndrop(strbuf *sb) ;

/**
 * @brief Drop any element that is a path descendant of a kept previous element.
 *
 * Treats elements as filesystem paths and, scanning in order, drops an element
 * when it is a child (in the `dir_is_child` sense) of the most recently kept
 * non-descendant element -- e.g. drops `"/foo/bar"` when `"/foo"` precedes it.
 * The literal element `"/"` is always skipped. @sb is assumed already sorted so
 * that a parent precedes its descendants. The result is rebuilt through a
 * scratch buffer.
 *
 * @param[in,out] sb Target sbl of path strings; assumed already sorted.
 * @return 1 on success.
 * @return 0 on failure; `errno` is the strbuf failure errno (`ERANGE` on size
 *         overflow, else the allocator errno) from building or copying the
 *         scratch buffer.
 * @see dir_is_child
 */
extern int sbl_dir_drop_child(strbuf *sb) ;

/**
 * @brief Replace the list by its elements joined with a single space.
 *
 * Concatenates every element separated by a single `' '`, with NO trailing
 * separator, and replaces @sb's content with that one C string. After the call
 * `sb->len` is the joined length EXCLUDING the terminating NUL, and `sb->s` is
 * NUL-terminated at `sb->s[sb->len]` (the NUL is not counted) -- i.e. @sb is no
 * longer a multi-element sbl but a single flat string.
 *
 * @param[in,out] sb Target sbl.
 * @return 1 on success.
 * @return 0 in two distinct cases:
 *      - the list is empty (`sb->len == 0`); `errno` is NOT set. An empty list
 *        is reported as 0, not 1.
 *      - building, copying or NUL-terminating the result failed; `errno` is the
 *        strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno).
 */
extern int sbl_rebuild_oneline(strbuf *sb) ;

/**
 * @brief Replace the list by its elements joined with `'\n'`, keeping a trailing one.
 *
 * Concatenates every element separated by `'\n'` and KEEPS the trailing `'\n'`,
 * replacing @sb's content with that one C string (NUL-terminated at `sb->len`,
 * the NUL not counted). The result is a multi-line text blob, not an sbl.
 *
 * @param[in,out] sb Target sbl.
 * @return 1 on success.
 * @return 0 in two distinct cases:
 *      - the list is empty (`sb->len == 0`); `errno` is NOT set.
 *      - building, copying or NUL-terminating the result failed; `errno` is the
 *        strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno).
 */
extern int sbl_rebuild_nline(strbuf *sb) ;

/**
 * @brief Replace the list by its elements joined with @delim.
 *
 * Concatenates every element separated by the single byte @delim, with NO
 * trailing separator, and replaces @sb's content with that one C string
 * (NUL-terminated at `sb->len`, the NUL not counted).
 *
 * @param[in,out] sb Target sbl.
 * @param[in] delim  Separator byte; only its low 8 bits are used (cast to `char`).
 * @return 1 on success.
 * @return 0 in two distinct cases:
 *      - the list is empty (`sb->len == 0`); `errno` is NOT set.
 *      - building, copying or NUL-terminating the result failed; `errno` is the
 *        strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno).
 */
extern int sbl_rebuild_with_delim(strbuf *sb, unsigned int delim) ;

/**
 * @brief Count the elements.
 *
 * Walks @sb with `FOREACH_SBL` and counts the NUL-separated elements. Pure read;
 * always succeeds and never sets `errno`. An empty list yields 0.
 *
 * @param[in] sb The sbl to measure.
 * @return The number of elements (0 for an empty list).
 */
extern size_t sbl_count(strbuf *sb) ;

/**
 * @brief Find the byte offset of the first element exactly equal to @s.
 *
 * Walks @sb with `FOREACH_SBL` and `strcmp`-compares each element to @s,
 * returning the offset of the first exact match. Pure read; never sets `errno`.
 *
 * @param[in] sb The sbl to search.
 * @param[in] s  NUL-terminated C string to match exactly.
 * @return The byte offset (>= 0) of the first element equal to @s.
 * @return -1 if no element equals @s. `errno` is not set.
 */
extern ssize_t sbl_search(strbuf *sb, char const *s) ;

/**
 * @brief Find the byte offset of the first occurrence of substring @str.
 *
 * Searches the raw buffer bytes for @str as a substring, scanning across element
 * boundaries (NUL bytes are ordinary bytes here, not delimiters). Returns the
 * offset of the first match. Pure read; never sets `errno`.
 *
 * @param[in] sb  The sbl whose raw bytes are searched.
 * @param[in] str NUL-terminated substring to look for.
 * @return The byte offset (>= 0) of the first match.
 * @return -1 if @sb is empty (`sb->len == 0`) or @str does not occur. An empty
 *         @str also yields -1 (it is treated as no match). `errno` is not set.
 * @note The match is not bounded to a single element: a hit may straddle two
 *       elements and include an embedded NUL. Use `sbl_search` for whole-element
 *       equality.
 */
extern ssize_t sbl_find(strbuf *sb, char const *str) ;

/**
 * @brief Find the byte offset of the element at index @id (0-based).
 *
 * Walks the elements counting indices and returns the byte offset of the
 * @id-th one. Pure read; never sets `errno`.
 *
 * @param[in] sb The sbl to index.
 * @param[in] id 0-based element index.
 * @return The byte offset (>= 0) of the element at index @id.
 * @return -1 if @id is out of range (>= element count, which includes the empty
 *         list). `errno` is not set.
 */
extern ssize_t sbl_element_byid(strbuf *sb, unsigned int id) ;

/**
 * @brief Copy the whole NUL-separated content into @dst.
 *
 * Copies exactly `sb->len` raw bytes (all elements and their NULs) from @sb into
 * @dst with `memcpy`. No allocation, no terminator added; always succeeds and
 * never sets `errno`. When @sb is empty (`sb->len == 0`) nothing is written.
 *
 * @param[out] dst Destination buffer; the caller MUST guarantee it has room for
 *                 at least `sb->len` bytes. Not bounds-checked.
 * @param[in] sb   Source sbl.
 * @note Sizing is the caller's responsibility: query `sb->len` before calling.
 *       An undersized @dst overflows.
 */
extern void sbl_to_char(char *dst, strbuf *sb) ;

/**
 * @brief Append the names in @srcdir whose type is in @flags, excluding @exclude.
 *
 * Validates @flags, opens @srcdir, then for each entry other than `.` and `..`
 * calls `fstatat(dirfd, name, &st, 0)`. That stat FOLLOWS symbolic links: the
 * type tested is the one of the link target, so `FLAGS_LNK` can never match
 * here and a symbolic link to a directory matches `FLAGS_DIR`. An entry whose
 * type bit (mode_to_flags() in types.h) is in @flags, the match being ANY of
 * the types in @flags, and whose name is not in @exclude is appended as a new
 * sbl element, without any path prefix. Not recursive. The order is the
 * `readdir()` order, which is unspecified.
 *
 * @param[in,out] sb Destination sbl; matching names are appended at `sb->len`.
 *                   May start empty (`sb->s == NULL`).
 * @param[in] srcdir Directory to read; passed to `opendir`.
 * @param[in] exclude NULL-terminated array of C strings to skip by exact name.
 *                   Must not be NULL and must end with a NULL pointer. It is
 *                   walked for every entry whose type matches.
 * @param[in] flags Non-empty OR of FLAGS_* bits (types.h), e.g.
 *                  `FLAGS_REG | FLAGS_DIR`. Any bit outside `FLAGS_ALL` is
 *                  refused, which includes every S_IF* value.
 * @return 1 on success (including when nothing matched).
 * @return 0 on failure:
 *      - `errno` is set to `EINVAL` if @flags is 0 or has a bit outside
 *        `FLAGS_ALL`. Checked before `opendir()`, @sb is not touched.
 *      - the `opendir(srcdir)` errno if the directory cannot be opened; @sb is
 *        not touched.
 *      - the `fstatat` errno if an entry cannot be stat'ed (e.g. `ENOENT` for a
 *        dangling symbolic link, `ELOOP` for a symbolic link loop);
 *      - the strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno) if appending a name fails.
 *      On the last two paths @sb is restored: it is freed if `sb->s` was NULL on
 *      entry, otherwise truncated back to its entry length. The directory is
 *      closed with dir_close(), which preserves `errno`.
 * @note Every entry is stat'ed before the type and @exclude tests, so a single
 *       entry that cannot be stat'ed fails the whole call, even if its name is
 *       in @exclude or its type would not match.
 * @note A `readdir()` error is not detected: it ends the scan like the end of
 *       the directory, and 1 is returned with the names collected so far.
 * @see mode_to_flags
 */
extern int sbl_dir_get(strbuf *sb, char const *srcdir, char const **exclude, uint32_t flags) ;

/**
 * @brief Recursively append names under @srcdir whose type is in @flags, excluding @exclude.
 *
 * Validates @flags, opens @srcdir, then for each entry other than `.` and `..`
 * calls `fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW)` (`lstat` semantics):
 * symbolic links are NOT followed. Unlike sbl_dir_get() and
 * sbl_dir_get_element(), a symbolic link is therefore seen as such, and this is
 * the only one of the three where `FLAGS_LNK` can match. For each entry:
 *      - if its name is in @exclude it is skipped: neither appended nor, for a
 *        directory, recursed into;
 *      - if it is a directory (`S_ISDIR`, so never a symbolic link to one), the
 *        path "@srcdir/name" is built and the function recurses into it with
 *        the same @exclude, @flags and @include_path. Recursion does not depend
 *        on @flags: every non-excluded subdirectory is descended into, even
 *        when `FLAGS_DIR` is not in @flags;
 *      - then, if its type bit (mode_to_flags()) is in @flags, the match being
 *        ANY of the types in @flags, it is appended.
 * When @include_path is non-zero each appended element is "srcdir/name", where
 * srcdir is the directory path of that level as built from the top @srcdir
 * (e.g. "top/sub/name"); otherwise the bare entry name is appended, at every
 * depth. A directory's descendants are appended BEFORE the directory itself.
 * Within a level the order is the `readdir()` order, which is unspecified.
 *
 * @param[in,out] sb Destination sbl; matches are appended at `sb->len`. May
 *                   start empty (`sb->s == NULL`).
 * @param[in] srcdir Directory to read; passed to `opendir`. Inserted verbatim
 *                   in the built paths, a trailing '/' yields "srcdir//name".
 * @param[in] exclude NULL-terminated array of names to skip exactly. Must not be
 *                   NULL and must end with a NULL pointer; it is walked for
 *                   every entry. Applies at every depth, to files and
 *                   subdirectories alike.
 * @param[in] flags Non-empty OR of FLAGS_* bits (types.h), e.g.
 *                  `FLAGS_REG | FLAGS_LNK`. Any bit outside `FLAGS_ALL` is
 *                  refused, which includes every S_IF* value.
 * @param[in] include_path Non-zero to prefix each appended name with its
 *                   directory path, zero to append the bare name.
 * @return 1 on success (including when nothing matched).
 * @return 0 on failure, at any recursion level:
 *      - `errno` is set to `EINVAL` if @flags is 0 or has a bit outside
 *        `FLAGS_ALL`. Checked before `opendir()`, @sb is not touched.
 *      - the `opendir` errno if a directory cannot be opened (for the top
 *        @srcdir, @sb is not touched);
 *      - the `fstatat` errno if an entry cannot be stat'ed;
 *      - the strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno) when building the "srcdir/name" path or appending a name fails.
 *      On any failure below the top level, @sb is restored to its state on
 *      entry of the top call: freed if `sb->s` was NULL then, otherwise
 *      truncated back to that length. The directories are closed with
 *      dir_close(), which preserves `errno`.
 * @note A single entry that cannot be stat'ed, anywhere in the tree, fails the
 *       whole call.
 * @note A `readdir()` error is not detected: it ends the scan of that directory
 *       like its end, and the walk goes on; 1 can be returned with an
 *       incomplete list.
 * @see sbl_dir_get
 * @see mode_to_flags
 */
extern int sbl_dir_get_recursive(strbuf *sb, char const *srcdir, char const **exclude, uint32_t flags, uint8_t include_path) ;

/**
 * @brief Append every entry of @srcdir named @element whose type is in @flags.
 *
 * Validates @flags, opens @srcdir, then for each entry other than `.` and `..`
 * calls `fstatat(dirfd, name, &st, 0)`. That stat FOLLOWS symbolic links: the
 * type tested is the one of the link target, so `FLAGS_LNK` can never match
 * here. An entry whose type bit (mode_to_flags()) is in @flags, the match
 * being ANY of the types in @flags, and whose name equals @element is
 * appended; when @include_path is non-zero it is appended as "srcdir/name".
 * Not recursive. (A single directory holds at most one entry of a given name,
 * so the count is 0 or 1 in practice.)
 *
 * @param[in,out] sb Destination sbl; matches are appended at `sb->len`. May
 *                   start empty (`sb->s == NULL`).
 * @param[in] srcdir Directory to read; passed to `opendir`. Inserted verbatim
 *                   in the appended path, a trailing '/' yields "srcdir//name".
 * @param[in] element Exact entry name to match. Must not be NULL.
 * @param[in] flags Non-empty OR of FLAGS_* bits (types.h). Any bit outside
 *                  `FLAGS_ALL` is refused, which includes every S_IF* value.
 * @param[in] include_path Non-zero to prefix the appended name with `srcdir/`.
 * @return The number of matching entries appended on success: a positive count,
 *         or 0 when the directory was scanned but no entry matched. The
 *         function does not set `errno` on these paths.
 * @return -1 on failure:
 *      - `errno` is set to `EINVAL` if @flags is 0 or has a bit outside
 *        `FLAGS_ALL`. Checked before `opendir()`, @sb is not touched.
 *      - the `opendir(srcdir)` errno if the directory cannot be opened; @sb is
 *        not touched.
 *      - the `fstatat` errno if an entry cannot be stat'ed (e.g. `ENOENT` for a
 *        dangling symbolic link);
 *      - the strbuf failure errno (`ERANGE` on size overflow, else the allocator
 *        errno) if appending fails.
 *      On the last two paths @sb is restored: freed if `sb->s` was NULL on
 *      entry, otherwise truncated back to its entry length. The directory is
 *      closed with dir_close(), which preserves `errno`.
 * @note Every entry is stat'ed before its name is compared to @element, so an
 *       unrelated entry that cannot be stat'ed fails the call with -1.
 * @note A `readdir()` error is not detected: it ends the scan like the end of
 *       the directory, and the count found so far is returned.
 * @see mode_to_flags
 */
extern int sbl_dir_get_element(strbuf *sb, char const *srcdir, char const *element, uint32_t flags, uint8_t include_path) ;

/**
 * @brief Tokenise @string into @sb, replacing @sb's content.
 *
 * Resets @sb to empty, then runs the lexer in "trim clean" mode over @string:
 * leading/trailing ` \t\r\n` are trimmed and the text is split on runs of
 * whitespace into one element per token. The result is a fresh sbl. Because the
 * content is replaced, @string must NOT alias `sb->s`.
 *
 * @param[out] sb  Destination sbl; its previous content is discarded.
 * @param[in] string NUL-terminated, non-empty input to tokenise. Must not alias
 *                   `sb->s`.
 * @return 1 on success.
 * @return 0 on failure:
 *      - `errno` is set to `EINVAL` if @string is empty (`*string == 0`). The
 *        EINVAL check runs before @sb is reset, so on this path @sb is left
 *        unchanged.
 *      - otherwise the lexer failed; `errno` is `EOVERFLOW` when the lexer's own
 *        buffer bound is exceeded, or the strbuf failure errno (`ERANGE` on size
 *        overflow, else the allocator errno) from growing @sb.
 * @see lexer_trim_clean
 */
extern int sbl_clean_string(strbuf *sb, char const *string) ;

/**
 * @brief Re-split @sb's content into one element per line.
 *
 * Reads `sb->s` as one C string and runs the lexer in "get line" mode, producing
 * one element per line. Works on a scratch copy to avoid aliasing, then copies
 * the result back into @sb.
 *
 * @param[in,out] sb Buffer whose current content is read as one C string and
 *                   replaced by the per-line element list.
 * @return 1 on success.
 * @return 0 in two distinct cases:
 *      - @sb is empty on entry (`sb->len == 0`); `errno` is NOT set.
 *      - the lexer or the copy-back failed; `errno` is `EOVERFLOW` when the
 *        lexer's buffer bound is exceeded, or the strbuf failure errno (`ERANGE`
 *        on size overflow, else the allocator errno).
 * @see lexer_trim_line
 */
extern int sbl_split_string_in_nline(strbuf *sb) ;

/**
 * @brief Replace every occurrence of the exact string @regex with @by, list-wide.
 *
 * For each element of @sb, replaces every occurrence of the literal string
 * @regex (not a real regular expression -- it is matched verbatim by the lexer)
 * with @by, and rebuilds @sb with the substituted elements. Recurses on the
 * remainder of an element after the first match to cover multiple hits per
 * element.
 *
 * @param[in,out] sb Target sbl; rebuilt with substitutions applied.
 * @param[in] regex NUL-terminated literal string to find. Must not be NULL.
 * @param[in] by   NUL-terminated replacement string. Must not be NULL.
 * @return 1 on success.
 * @return 0 on failure:
 *      - `errno` is set to `EINVAL` if @sb is empty (`sb->len == 0`), @regex is
 *        NULL, @by is NULL, or @regex equals @by (`strcmp(regex, by) == 0`).
 *      - otherwise a lexer or strbuf step failed; `errno` is `EOVERFLOW` when the
 *        lexer's buffer bound is exceeded, or the strbuf failure errno (`ERANGE`
 *        on size overflow, else the allocator errno).
 */
extern int sbl_replace(strbuf *sb, char const *regex, char const *by) ;

/**
 * @brief Replace @regex with @by across a `'\n'`-joined text blob in @sb.
 *
 * Treats @sb as a multi-line text buffer rather than an sbl list: splits it into
 * one element per line (`sbl_split_string_in_nline`), runs `sbl_replace` on the
 * elements, then rejoins them with `'\n'` keeping a trailing `'\n'`
 * (`sbl_rebuild_nline`). The net effect is a literal find/replace over text,
 * consuming and producing a multi-line buffer.
 *
 * @param[in,out] sb Multi-line text buffer to transform in place.
 * @param[in] regex NUL-terminated literal string to find. Must not be NULL.
 * @param[in] by   NUL-terminated replacement string. Must not be NULL.
 * @return 1 on success.
 * @return 0 if any of the three composed steps fails; `errno` follows the
 *         failing step:
 *      - unset if `sb` was empty (the split step returns 0 with no errno);
 *      - `EINVAL` if `sbl_replace`'s argument check rejects (empty list, NULL
 *        @regex, NULL @by, or @regex == @by);
 *      - otherwise `EOVERFLOW` (lexer bound) or the strbuf failure errno
 *        (`ERANGE` on size overflow, else the allocator errno).
 * @see sbl_split_string_in_nline
 * @see sbl_replace
 * @see sbl_rebuild_nline
 */
extern int sbl_replace_nline(strbuf *sb, char const *regex, char const *by) ;

#endif
