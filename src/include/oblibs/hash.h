/*
 * hash.h
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
 * @brief Intrusive, chained hash table.
 *
 * This module provides a hash table that stores no copies of the caller's elements. The
 * caller embeds a `hash_node_t` inside its own element structure and registers, once at
 * init, the offset of that embedded node (`node_off`). The table links elements through
 * those embedded nodes; the elements themselves are never copied and are never freed by
 * the table.
 *
 * Key properties, as implemented:
 *
 *  - Intrusive nodes. One table corresponds to exactly one node offset, fixed at init: every
 *    element added to a given table must embed its `hash_node_t` at that same offset. An
 *    element may live in several tables at once by embedding one distinct node per table,
 *    each table initialised with the matching offset.
 *
 *  - Keys are not copied. `hash_add()` stores the key POINTER and its length, not the bytes.
 *    The keyed bytes must remain valid and unchanged for as long as the element stays in the
 *    table; the natural way to satisfy this is to key on memory that lives inside the element
 *    itself. The cached 32-bit hash is computed at add time (FNV-1a), so mutating the keyed
 *    bytes while the element is in the table breaks lookup.
 *
 *  - Power-of-two bucket count. The table indexes buckets by `hashv & (nbuckets - 1)`, which
 *    requires `nbuckets` to be a power of two at all times. `hash_init()` rounds the requested
 *    capacity up to a power of two (minimum 8); growth doubles the count. Collisions are
 *    resolved by a doubly-linked chain per bucket, so removal by element is O(1) without
 *    rescanning the bucket. The table grows by doubling and rehashing when adding one more
 *    element would make `count` exceed `nbuckets * HASH_MAX_LOAD`. Sizing the table to the
 *    expected element count at init avoids any growth.
 *
 *  - Insertion-order iteration. Beyond the per-bucket chains, every node is also linked into a
 *    global insertion-order list (`ahead`..`atail`). `HASH_FOREACH` walks that list, so
 *    traversal order is the order in which elements were added, independent of the hash
 *    function and unchanged by any growth (rehashing rebuilds only the bucket chains).
 *
 *  - Error handling. Functions that can fail return 0 (or NULL) and set `errno` to `EINVAL`
 *    or `ENOMEM`. The void functions (`hash_free`, `hash_del`) never set `errno`.
 *
 * @note The table performs no copying and no element freeing. `hash_free()` releases only the
 *       bucket array; the caller owns and must free the elements. The module provides no
 *       thread-safety: concurrent access must be serialised by the caller.
 */

#ifndef OBLIBS_HASH2_H
#define OBLIBS_HASH2_H

#include <stddef.h>
#include <stdint.h>

/**
 * @struct hash_node_s
 * @brief Intrusive table node, embedded by the caller inside its own element.
 *
 * One `hash_node_t` ties an element into one table. It carries the two doubly-linked chains
 * the table maintains (the per-bucket collision chain and the global insertion-order chain),
 * plus the cached key reference and hash. The node is owned and stored in place by the caller;
 * the table only links and unlinks it. Its fields are managed entirely by the table while the
 * element is in it: the caller writes none of them directly. Initialise an embedded node to
 * `HASH_NODE_ZERO` before first use is not required by `hash_add()` (which overwrites the
 * relevant fields), but is good hygiene for elements that may be tested with iteration macros
 * before insertion.
 *
 * @param hnext
 * Next node in the same bucket's collision chain, or NULL at the tail. Set by the table.
 *
 * @param hprev
 * Previous node in the same bucket's collision chain, or NULL at the head. Set by the table.
 *
 * @param anext
 * Next node in the global insertion-order chain, or NULL at the most-recently-added end.
 * Set by the table; walked by `HASH_FOREACH`.
 *
 * @param aprev
 * Previous node in the global insertion-order chain, or NULL at the oldest end. Set by the table.
 *
 * @param key
 * Pointer to the key bytes, as passed to `hash_add()`. The bytes are NOT copied or owned; the
 * table stores only this pointer. Cleared to NULL by `hash_del()`.
 *
 * @param keylen
 * Length of the key in bytes, narrowed from the `size_t keylen` given to `hash_add()` to
 * `uint32_t`.
 *
 * @param hashv
 * 32-bit FNV-1a hash of the key, cached at add time and reused for lookup, bucket selection
 * and rehashing.
 */
typedef struct hash_node_s hash_node_t ;
struct hash_node_s
{
    hash_node_t *hnext ;
    hash_node_t *hprev ;
    hash_node_t *anext ;
    hash_node_t *aprev ;
    const void *key ;
    uint32_t keylen ;
    uint32_t hashv ;
} ;

/** @brief Zero initialiser for an embedded `hash_node_t` (all pointers NULL, lengths/hash 0). */
#define HASH_NODE_ZERO { NULL, NULL, NULL, NULL, NULL, 0, 0 }

/**
 * @struct hash_s
 * @brief An intrusive, chained hash table.
 *
 * The table owns a dynamically allocated array of bucket heads and threads the caller's
 * elements through the `hash_node_t` each embeds. It maintains two views of the same set of
 * nodes: per-bucket collision chains (for O(1) lookup and removal) and one global
 * insertion-order chain (for stable iteration). Invariants the implementation relies on:
 * `nbuckets` is always a power of two (so `hashv & (nbuckets - 1)` is the bucket index);
 * `count` is the exact number of elements currently linked; `node_off` is fixed for the life
 * of the table and shared by all its elements. A zeroed table (`HASH_ZERO`) is NOT usable:
 * `hash_init()` must be called first, since `buckets` must be non-NULL before any add or find.
 *
 * @param buckets
 * Array of `nbuckets` bucket heads, each the head of a doubly-linked collision chain (NULL when
 * empty). Allocated by `hash_init()`/`hash_grow()`, freed and set to NULL by `hash_free()`. A
 * NULL `buckets` marks the table as uninitialised and is rejected by `hash_find`/`hash_add`.
 *
 * @param ahead
 * Head (oldest element) of the global insertion-order chain, or NULL when the table is empty.
 *
 * @param atail
 * Tail (most-recently-added element) of the global insertion-order chain, or NULL when empty.
 *
 * @param nbuckets
 * Number of buckets. Always a power of two, at least `HASH_MIN_BUCKETS` (8), doubled on growth.
 *
 * @param count
 * Number of elements currently held in the table.
 *
 * @param node_off
 * Offset, within the caller's element structure, of the embedded `hash_node_t`. Set once at
 * init and used by every element of this table to convert between element and node.
 */
typedef struct hash_s hash_t ;
struct hash_s
{
    hash_node_t **buckets ;
    hash_node_t *ahead ;
    hash_node_t *atail ;
    size_t nbuckets ;
    size_t count ;
    size_t node_off ;
} ;

/** @brief Zero initialiser for a `hash_t`. The result is NOT usable until `hash_init()`. */
#define HASH_ZERO { NULL, NULL, NULL, 0, 0, 0 }

/**
 * @brief Load-factor numerator: the table doubles when adding one more element would make
 *        `count` exceed `nbuckets * HASH_MAX_LOAD`.
 *
 * With the value 2 the table grows once the average chain length would pass 2, keeping lookups
 * short. It is a plain integer constant used directly in the growth test in `hash_add()`.
 */
#define HASH_MAX_LOAD 2

/**
 * @brief Initialise an empty table.
 *
 * Allocates the bucket array and records the node offset. The bucket count is
 * `capacity` rounded up to a power of two, with a floor of `HASH_MIN_BUCKETS` (8); a
 * `capacity` of 0 therefore yields 8 buckets. Sizing `capacity` to the expected element count
 * avoids later growth. On success the table is empty (`count` 0, both insertion-order ends
 * NULL). `node_off` is stored verbatim and is not validated.
 *
 * @param[out] h        The table to initialise. Must not be NULL. On success all fields are
 *                      set; on failure `h` is left unmodified.
 * @param[in]  capacity Expected number of elements; rounded up to a power of two (minimum 8)
 *                      for the bucket count. 0 selects the minimum of 8 buckets.
 * @param[in]  node_off Offset, within the caller's element structure, of the embedded
 *                      `hash_node_t` (use `offsetof`). Every element added to this table must
 *                      embed its node at this offset.
 * @return 1 on success.
 * @return 0 on error (errno set):
 *      - errno is set to `EINVAL` if @h is NULL.
 *      - errno is set to `ENOMEM` if the bucket-array allocation (`calloc`) fails.
 *
 * @retval 1 Table initialised and ready for `hash_add`/`hash_find`.
 * @retval 0 errno is set to `EINVAL` (NULL @h) or `ENOMEM` (allocation failure).
 */
extern int hash_init(hash_t *h, size_t capacity, size_t node_off) ;

/**
 * @brief Release the table's own memory (the bucket array) and zero the table.
 *
 * Frees the bucket array and resets every field of @h to its zero value (equivalent to
 * `HASH_ZERO`). The elements are NOT freed and their embedded nodes are NOT cleared: ownership
 * of the elements stays with the caller, who must release them itself. After this call @h is
 * back to the uninitialised state and must be re-initialised with `hash_init()` before reuse.
 *
 * @param[in,out] h The table to free. A NULL pointer is a no-op.
 *
 * @note This function never sets `errno`.
 * @note The embedded `hash_node_t` of any still-referenced element keeps stale link pointers
 *       after this call; do not iterate or `hash_del` such elements against the freed table.
 */
extern void hash_free(hash_t *h) ;

/**
 * @brief Look up an element by key.
 *
 * Computes the key's hash, scans the matching bucket chain, and returns the first element
 * whose cached hash, key length and key bytes all match (compared with `memcmp`). Lookup of an
 * empty table (`count` 0) short-circuits to a miss before the key is examined.
 *
 * @param[in] h      The table. Must be initialised (`buckets` non-NULL).
 * @param[in] key    Pointer to the key bytes to match. Dereferenced only when @h is non-empty;
 *                   must then be a valid pointer to at least @keylen bytes (no NULL check is
 *                   performed for a non-empty table).
 * @param[in] keylen Length of the key in bytes.
 * @return Pointer to the matching element on a hit; NULL on a miss or on error.
 *      - On a hit: the address of the caller's element (the embedded node minus `node_off`).
 *      - On a clean miss (no matching key, or the table is empty): NULL, and `errno` is left
 *        untouched.
 *      - On error: NULL with `errno` set to `EINVAL` if @h is NULL or the table is
 *        uninitialised (`buckets` NULL).
 *
 * @retval NULL Ambiguous between "not found" and "invalid table": the two are distinguished
 *         only via `errno`. To tell them apart, set `errno = 0` before the call; a NULL return
 *         with `errno == EINVAL` is the error case, NULL with `errno` unchanged is a miss.
 *
 * @note A non-empty table with a NULL @key dereferences @key (undefined); the NULL-key guard
 *       lives in `hash_add()`, not here.
 */
extern void *hash_find(hash_t *h, const void *key, size_t keylen) ;

/**
 * @brief Add an element to the table.
 *
 * Stores the key pointer and length in the element's embedded node (the bytes are not copied),
 * caches the key's hash, links the node at the head of its bucket chain and at the tail of the
 * insertion-order list, and increments `count`. If adding one more element would make `count`
 * exceed `nbuckets * HASH_MAX_LOAD`, the table first doubles its bucket count and rehashes; if
 * that growth allocation fails, the element is NOT added.
 *
 * The element must embed its `hash_node_t` at the offset registered at init, and the keyed
 * bytes must remain valid and unchanged for as long as the element stays in the table (the
 * hash is cached at this point). Adding an element already present in this table, or re-adding
 * one without an intervening `hash_del`, corrupts the chains; callers that may re-add must
 * `hash_find` (or track membership) first. This function does not detect duplicate keys: two
 * elements with equal keys can coexist, and `hash_find` returns whichever is first in its
 * bucket chain.
 *
 * @param[in]     h      The table. Must be initialised (`buckets` non-NULL).
 * @param[in]     key    Pointer to the key bytes; the pointer and length are stored, the bytes
 *                       are not copied. Must not be NULL.
 * @param[in]     keylen Length of the key in bytes. Must be non-zero; stored narrowed to
 *                       `uint32_t`.
 * @param[in,out] elem   Pointer to the caller's element; its embedded `hash_node_t` is written.
 *                       Must not be NULL.
 * @return 1 on success (element added, `count` incremented).
 * @return 0 on error (errno set):
 *      - errno is set to `EINVAL` if @h is NULL, the table is uninitialised (`buckets` NULL),
 *        @key is NULL, @keylen is 0, or @elem is NULL.
 *      - errno is set to `ENOMEM` if the table needed to grow and the new bucket array's
 *        allocation (`calloc`) failed; in this case the element is NOT added and the table is
 *        left unchanged.
 *
 * @retval 1 Element linked into the bucket chain and the insertion-order list.
 * @retval 0 errno is set to `EINVAL` (bad argument / uninitialised table) or `ENOMEM` (growth
 *         allocation failure, element not added).
 */
extern int hash_add(hash_t *h, const void *key, size_t keylen, void *elem) ;

/**
 * @brief Remove an element from the table.
 *
 * Unlinks @elem from both its bucket chain and the insertion-order list, clears the link
 * pointers and the key pointer of its embedded node, and decrements `count`. The element's
 * memory is not freed; ownership stays with the caller, who may then reuse or free it.
 *
 * @param[in,out] h    The table. A NULL @h or an uninitialised table (`buckets` NULL) makes the
 *                      call a no-op.
 * @param[in,out] elem Pointer to an element currently held by the table; its embedded node is
 *                      cleared. A NULL @elem makes the call a no-op. Passing an element NOT
 *                      currently in this table is undefined: it corrupts the chains and
 *                      decrements `count` wrongly.
 *
 * @note This function never sets `errno`. It does not verify that @elem belongs to @h.
 */
extern void hash_del(hash_t *h, void *elem) ;

/**
 * @brief Number of elements currently held.
 *
 * @param[in] h The table, or NULL.
 * @return The current element count; 0 if @h is NULL. Always succeeds; never sets `errno`.
 */
extern size_t hash_count(const hash_t *h) ;

/**
 * @brief Address of the `hash_node_t` embedded in element @el for table @h.
 *
 * Converts an element pointer to its embedded node by adding `h->node_off`. @el must be
 * non-NULL: this macro applies no NULL guard and would compute an invalid address for NULL.
 *
 * @param h  Pointer to the table (read for `node_off`).
 * @param el Pointer to a caller element (must not be NULL).
 * @return Pointer to the `hash_node_t` embedded in @el.
 * @see HASH_ELT_OF
 */
#define HASH_NODE_OF(h, el) ((hash_node_t *)(void *)((char *)(el) + (h)->node_off))

/**
 * @brief Element owning `hash_node_t` @n for table @h, or NULL.
 *
 * Inverse of `HASH_NODE_OF`: subtracts `h->node_off` from the node address. NULL-safe: a NULL
 * @n yields NULL, which is how iteration and `hash_find` represent "no element".
 *
 * @param h Pointer to the table (read for `node_off`).
 * @param n Pointer to an embedded node, or NULL.
 * @return Pointer to the owning element, or NULL if @n is NULL.
 * @see HASH_NODE_OF
 */
#define HASH_ELT_OF(h, n) ((n) ? (void *)((char *)(n) - (h)->node_off) : NULL)

/**
 * @brief Iterate elements in insertion order, safe against deletion of the current element.
 *
 * Walks the global insertion-order list from oldest to most recently added. The next element
 * is captured into @tmp before the loop body runs, so the body may `hash_del(h, el)` the
 * current element without breaking the walk. Adding elements during iteration appends them to
 * the tail and they will be visited later in the same loop; that is a consequence of the
 * insertion-order list, not a guarantee to rely on.
 *
 * @param h   Pointer to the table.
 * @param el  Pointer variable of the caller's element type; set to the current element each
 *            iteration.
 * @param tmp Pointer variable of the caller's element type; scratch holding the next element.
 */
#define HASH_FOREACH(h, el, tmp) \
    for ((el) = HASH_ELT_OF((h), (h)->ahead) ; \
         (el) != NULL && ((tmp) = HASH_ELT_OF((h), HASH_NODE_OF((h), (el))->anext), 1) ; \
         (el) = (tmp))

#endif
