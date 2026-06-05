/*
 * strbuf.h
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
 * Growable byte/string buffer -- the oblibs-native replacement for skalibs'
 * stralloc. Differences from stralloc, on purpose:
 *
 *   - Geometric 1.5x growth instead of skalibs' +12.5%: far fewer reallocs on
 *     append-heavy paths, while still amortized O(1) and friendly to reuse of
 *     freed blocks.
 *   - No zeroing of the allocated slack. Bytes in [len, cap) are UNDEFINED;
 *     callers must not read past len. Append a NUL explicitly with strbuf_end()
 *     when a C string is needed. This drops stralloc's per-grow memset.
 *   - Direct malloc/realloc, no allocator indirection.
 *   - Optional borrowed storage (the `heap` flag): a strbuf may be seeded with
 *     caller-provided storage (e.g. on the C stack via _alloc_strbuf_) and only
 *     migrates to the heap the first time it overflows that storage. Small,
 *     bounded buffers then never touch the heap; the storage lives OUTSIDE the
 *     struct, so unlike an embedded small-buffer optimization `.s` is never a
 *     self-pointer.
 *
 * Copy semantics are shallow, exactly like stralloc: pass strbuf by pointer,
 * never by value.
 */

#ifndef OB_STRBUF_H
#define OB_STRBUF_H

#include <string.h>
#include <sys/uio.h>
#include <stdint.h>

typedef struct strbuf_s strbuf, *strbuf_ref ;
struct strbuf_s
{
    char *s ;       // data (NULL when empty and never seeded)
    size_t len ;    // bytes used
    size_t cap ;    // bytes available at s
    uint8_t heap ;  // 1 if s is malloc-owned (realloc/free it), 0 if borrowed
} ;

#define STRBUF_ZERO { .s = 0, .len = 0, .cap = 0, .heap = 0 }

/** Cap of the on-stack seed produced by _alloc_strbuf_ (bytes). */
#define STRBUF_STKMAX 2048

/** Automatic strbuf_free() at scope exit. */
#define _cleanup_strbuf_ __attribute__((cleanup(strbuf_free)))

/**
 * @brief Declare a scope-local strbuf seeded with on-C-stack storage of @n bytes.
 *
 * The buffer uses the stack seed until it overflows, then migrates to the heap.
 * It is freed automatically at scope exit. For a known-big buffer use
 * STRBUF_ZERO directly (pure heap). @n >= STRBUF_STKMAX falls back to a 1-byte
 * seed (heap on first real write).
 */
#define _alloc_strbuf_(sb, n) \
    _cleanup_strbuf_ strbuf sb = STRBUF_ZERO ; \
    char sb##__seed__[((n) > 0 && (n) < STRBUF_STKMAX) ? (n) : 1] ; \
    strbuf_with(&sb, sb##__seed__, sizeof(sb##__seed__))

/**
 * @brief Set up a strbuf with caller-provided storage (borrowed, not owned).
 *
 * The buffer writes into @storage until it overflows, then allocates on the
 * heap and copies. @storage is never freed by the strbuf. Use this to back a
 * strbuf with a static/global buffer or a buffer embedded in another struct;
 * for a fresh on-stack seed prefer _alloc_strbuf_.
 *
 * @return 1 on success, 0 on bad arguments (errno == EINVAL).
 */
extern int strbuf_with(strbuf *sb, void *storage, size_t size) ;

/**
 * @brief Ensure total capacity is at least @n bytes (like vector::reserve).
 *
 * Grows geometrically (1.5x) when it must, honoring a larger explicit @n, and
 * migrates borrowed storage to the heap on first overflow. Bytes in [len, cap)
 * stay UNDEFINED. To pre-grow for @k more bytes, call strbuf_reserve(sb, len+k).
 *
 * @return 1 on success, 0 on allocation failure / size overflow (errno == ERANGE).
 */
extern int strbuf_reserve(strbuf *sb, size_t n) ;

/** @brief Replace the contents with @n bytes from @s. @return 1 / 0 (errno). */
extern int strbuf_copyb(strbuf *sb, char const *s, size_t n) ;

/** @brief Append @n bytes from @s. @return 1 / 0 (errno). */
extern int strbuf_catb(strbuf *sb, char const *s, size_t n) ;

/** @brief Append the concatenation of @n iovec buffers. @return 1 / 0 (errno). */
extern int strbuf_catv(strbuf *sb, struct iovec const *v, unsigned int n) ;

/** @brief Insert @n bytes from @s at @offset (0..len). @return 1 / 0 (errno). */
extern int strbuf_insertb(strbuf *sb, size_t offset, char const *s, size_t n) ;

/** @brief Release heap storage (if owned) and reset to STRBUF_ZERO. */
extern void strbuf_free(strbuf *sb) ;

/** @brief Shrink owned heap storage down to len. No-op on borrowed storage.
 *  @return 1 on success, 0 on allocation failure. */
extern int strbuf_shrink(strbuf *sb) ;

#define strbuf_copys(sb, s) strbuf_copyb(sb, (s), strlen(s))
#define strbuf_cats(sb, s) strbuf_catb(sb, (s), strlen(s))
#define strbuf_copy(sb1, sb2) strbuf_copyb(sb1, (sb2)->s, (sb2)->len)
#define strbuf_cat(sb1, sb2) strbuf_catb(sb1, (sb2)->s, (sb2)->len)
#define strbuf_inserts(sb, offset, s) strbuf_insertb(sb, offset, (s), strlen(s))
#define strbuf_insert(sb1, offset, sb2) strbuf_insertb(sb1, offset, (sb2)->s, (sb2)->len)
#define strbuf_end(sb) strbuf_catb(sb, "", 1)

#endif
