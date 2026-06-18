/*
 * fhash.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 */

/**
 * @brief Fixed-capacity, zero-malloc, open-addressing hash table.
 *
 * This is the static counterpart of oblibs' chained `htab`: where `htab` owns a
 * heap bucket array and grows by reallocation, `fhash` NEVER allocates. The caller
 * provides the whole backing storage once, at init, sized to the maximum number of
 * entries it will ever hold. This is the model a long-lived PID 1 process with a
 * bounded service count needs, where the memory ceiling is a feature and a malloc
 * in the steady state is unacceptable.
 *
 * Design:
 *
 *  - Open addressing, linear probing:
 *    Entries live directly in the caller's slot array. A lookup hashes the key, jumps
 *    to its home slot, and walks forward (wrapping) until it finds the key or an empty
 *    slot. No chains, no per-entry node, no pointers to chase. The hash is FNV-1a,
 *    32-bit, computed over the raw key bytes.
 *
 *  - Keys are NOT copied:
 *    The table memorises the key POINTER and its length, like `htab`. The keyed bytes
 *    must stay valid and unchanged while the entry lives in the table. The natural way
 *    to satisfy this is to key on a field that lives inside a long-lived element (e.g.
 *    `pid` or `devino` inside a pooled service struct).
 *
 *  - Tombstone-free deletion (the robustness point):
 *    Naive open-addressing deletion leaves a "tombstone" marker so probe chains stay
 *    intact. Tombstones accumulate forever; on a process that runs for years with
 *    constant key churn (a service's pid changes on every restart), they would slowly
 *    poison every lookup until a full rehash. `fhash` instead uses Knuth's backward-shift
 *    deletion (Algorithm 6.4R): on removal it pulls the displaced cluster back into the
 *    hole, leaving the table exactly as if the entry had never been inserted. Probe
 *    performance is therefore stable for the entire uptime, with no rehash, ever.
 *
 *  - Load factor:
 *    The slot count is always a power of two. The table keeps at least one empty slot at
 *    all times -- `fhash_insert` refuses a new key once `count` reaches `n - 1` -- so
 *    load < 1 holds structurally and every probe loop is guaranteed to terminate, however
 *    the caller sized the table. `fhash_sizefor` is still recommended to keep the load at
 *    or below ~0.67 for good probe performance.
 *
 *  - Error handling:
 *    Failures are reported by a 0 return with errno set (`EINVAL`, `ENOSPC`). A clean
 *    lookup miss is a 0 return with errno left untouched.
 *
 * The value stored is a single `uint32_t` caller payload. For svscan it is the index of
 * the service in its pool (genset slot), exactly the integer the two `avltreen` return today.
 *
 * @note These functions do NOT validate `h` or `key` for NULL except where stated
 *       (`fhash_init` and `fhash_count`). `fhash_search`, `fhash_insert` and
 *       `fhash_delete` assume a table previously initialised by `fhash_init` and a
 *       non-NULL `key`; passing NULL, or an uninitialised/zeroed `fhash_t` (whose
 *       `slots` is NULL and `mask` is 0), is undefined behaviour.
 */

#ifndef OBLIBS_FHASH_H
#define OBLIBS_FHASH_H

#include <stdint.h>

/**
 * @struct fhash_slot_s
 * @brief One slot of the open-addressed table; holds at most one entry.
 *
 * Slots live in the caller-provided backing array. A slot is empty iff `key` is
 * NULL; this is the sentinel the probe loops stop on, so a live entry must never
 * carry a NULL `key`. The slot stores a borrowed pointer to the key bytes, not a
 * copy: the pointed-to bytes must stay valid and unchanged for the entry's lifetime.
 *
 * @param key
 * Borrowed pointer to the key bytes. NULL marks an empty slot.
 *
 * @param keylen
 * Length of the key in bytes.
 *
 * @param hash
 * Cached FNV-1a hash of the key, used for fast rejection before the byte compare.
 *
 * @param value
 * Caller payload stored under this key (typically a pool index).
 */
typedef struct fhash_slot_s fhash_slot_t ;
struct fhash_slot_s
{
    void const *key ;   // NULL marks an empty slot
    uint32_t keylen ;
    uint32_t hash ;     // cached hash of the key, for fast rejection
    uint32_t value ;    // caller payload (typically a pool index)
} ;

/**
 * @brief Zero initialiser for an `fhash_slot_t`, marking the slot empty.
 *
 * Sets every field to 0, so `key` is NULL (the empty-slot sentinel).
 */
#define FHASH_SLOT_ZERO { 0, 0, 0, 0 }

/**
 * @struct fhash_s
 * @brief Header describing the table over a caller-provided slot array.
 *
 * Holds no storage of its own: `slots` points at the caller's array of `n` slots,
 * which must outlive the table. `n` is always a power of two and `mask` is `n - 1`,
 * used to fold a hash onto a slot index with a bitwise AND. The invariant
 * `count <= n - 1` is maintained by `fhash_insert` (which refuses a new key at
 * `count >= n - 1`) so at least one free slot always exists and every probe loop
 * terminates.
 *
 * @param slots
 * Caller-provided array of `n` slots. Borrowed, never freed by this module.
 *
 * @param n
 * Slot count. Always a power of two and strictly greater than 0.
 *
 * @param mask
 * `n - 1`. Bitmask that maps a hash to a slot index (replaces a modulo).
 *
 * @param count
 * Number of live entries currently in the table. Always strictly less than `n`.
 */
typedef struct fhash_s fhash_t ;
struct fhash_s
{
    fhash_slot_t *slots ;   // caller-provided array of 'n' slots
    uint32_t n ;            // slot count, a power of two
    uint32_t mask ;         // n - 1, for the modulo
    uint32_t count ;        // live entries
} ;

/**
 * @brief Zero initialiser for an `fhash_t`.
 *
 * Sets every field to 0. Such a header is NOT usable as a table: `slots` is NULL
 * and `n` is 0. It must be passed through `fhash_init` (or have its fields set
 * directly) before any lookup, insert or delete. `fhash_count` accepts it and
 * returns 0.
 */
#define FHASH_ZERO { 0, 0, 0, 0 }

/**
 * @brief Smallest power-of-two slot count that holds `capacity` entries at load <= ~0.67.
 *
 * Computes `need = capacity + capacity/2` (~1.5x), then returns the smallest power of
 * two strictly greater than `need`, starting the search at 2. Use this to size the slot
 * array handed to `fhash_init`. For a usable `capacity` the result is always >= 2 (for
 * `capacity` 0 or 1 it returns 2) and is always strictly greater than `need`, so the
 * table's load stays at or below ~0.67 at full capacity. This function never sets errno.
 *
 * @param[in] capacity Maximum number of entries the table will ever hold.
 * @return The slot count to allocate: a power of two, at least 2, strictly greater
 *         than `capacity + capacity/2`. Returns 0 when no `uint32_t` power of two can
 *         hold that target (`capacity + capacity/2 >= 2^31`); `fhash_init` then rejects
 *         the 0 with EINVAL.
 *
 * @note The 1.5x target is computed in 64-bit and the candidate is bounded, so there is
 *       no overflow and the search always terminates: an out-of-range `capacity` yields
 *       a clean 0 return, never a wrapped value or a non-terminating loop.
 */
extern uint32_t fhash_sizefor (uint32_t capacity) ;

/**
 * @brief Initialise an empty table over caller-provided storage.
 *
 * Does not allocate. On success, points `h` at `slots`, records `n` and `mask = n - 1`,
 * sets `count` to 0, and zeroes the whole slot array (`memset` over `n * sizeof slot`),
 * which marks every slot empty. The backing array `slots` is borrowed: it must outlive
 * the table and is never freed by this module.
 *
 * @param[out] h     The table header to initialise. Must not be NULL.
 * @param[in]  slots Caller-owned array of `n` slots. Must not be NULL and must outlive
 *                   the table.
 * @param[in]  n     Number of slots. Must be non-zero and a power of two, and should be
 *                   strictly greater than the maximum number of entries (use
 *                   `fhash_sizefor`).
 * @return Status code:
 *      - 1 on success; the table is empty and ready for use
 *      - 0 on failure; errno is set to EINVAL
 *
 * @retval 0 errno is set to EINVAL if `h` is NULL, `slots` is NULL, `n` is 0, or `n`
 *         is not a power of two (`n & (n - 1)` is non-zero). On this path `h` is left
 *         unmodified.
 * @see fhash_sizefor
 */
extern int fhash_init(fhash_t *h, fhash_slot_t *slots, uint32_t n) ;

/**
 * @brief Look up a key.
 *
 * Hashes `key` (FNV-1a over `keylen` bytes), folds to a home slot, and walks forward
 * with wraparound until it either matches the key or hits an empty slot. A match
 * requires equal cached hash, equal length, and equal bytes (`memcmp`). The walk is
 * guaranteed to terminate because the table maintains at least one empty slot.
 *
 * @param[in]  h      The table. Must not be NULL and must have been initialised by
 *                    `fhash_init`.
 * @param[in]  key    Pointer to the key bytes to match. Must not be NULL when `keylen`
 *                    is non-zero.
 * @param[in]  keylen Length of the key in bytes.
 * @param[out] value  If non-NULL and the key is found, receives the stored payload.
 *                    Left untouched on a miss, and ignored if NULL.
 * @return Status code:
 *      - 1 if the key is found; `*value` is set when `value` is non-NULL
 *      - 0 on a clean miss; errno is left untouched and `*value` is not written
 *
 * @note Never sets errno and has no failure return distinct from "not found". Does not
 *       check `h` or `key` for NULL: passing a NULL `h`, a NULL `key` with non-zero
 *       `keylen`, or an uninitialised table is undefined behaviour.
 */
extern int fhash_search(fhash_t const *h, void const *key, uint32_t keylen, uint32_t *value) ;

/**
 * @brief Insert a key, or update its payload if already present.
 *
 * If `key` already exists, its stored payload is overwritten in place with `value`
 * and `count` is unchanged. Otherwise a new entry is placed in the first empty slot
 * on the probe path: the slot records the BORROWED `key` pointer, `keylen`, the cached
 * hash and `value`, and `count` is incremented. The key bytes are NOT copied; they
 * must stay valid and unchanged while the entry lives in the table.
 *
 * Before probing, the function refuses if no empty slot remains (`count >= n`); this
 * is the safety net that keeps the probe loop finite. When `n` was derived from
 * `fhash_sizefor` for the intended capacity, this fullness check never triggers.
 *
 * @param[in] h      The table. Must not be NULL and must have been initialised by
 *                   `fhash_init`.
 * @param[in] key    Pointer to the key bytes. Must not be NULL when `keylen` is
 *                   non-zero.
 * @param[in] keylen Length of the key in bytes.
 * @param[in] value  Payload to store under the key.
 * @return Status code:
 *      - 1 on success, whether the key was newly inserted or an existing entry's
 *        payload was updated
 *      - 0 on failure; errno is set to ENOSPC
 *
 * @retval 0 errno is set to ENOSPC if inserting a new key would consume the last free
 *         slot (`count >= n - 1`); the table holds at most `n - 1` entries so a free slot
 *         is always reserved for probe termination. Updating an already-present key always
 *         succeeds. Cannot happen for a new key when `n` was sized with `fhash_sizefor`.
 *         On this path the table is left unchanged.
 *
 * @note Does not check `h` or `key` for NULL: passing a NULL `h`, a NULL `key` with
 *       non-zero `keylen`, or an uninitialised table is undefined behaviour.
 * @see fhash_sizefor
 */
extern int fhash_insert(fhash_t *h, void const *key, uint32_t keylen, uint32_t value) ;

/**
 * @brief Remove a key, keeping the table tombstone-free (backward-shift).
 *
 * Locates `key` by the same probe walk as `fhash_search`. If found, removes it using
 * Knuth's backward-shift deletion (Algorithm 6.4R): the freed slot becomes a hole,
 * the following cluster is scanned, and each element whose home slot lies on the
 * hole's probe path is pulled back into the hole, moving the hole forward, until an
 * empty slot ends the cluster. `count` is decremented. This leaves the table exactly
 * as if `key` had never been inserted: no tombstone is created, so probe performance
 * does not degrade over time. The borrowed key pointer is simply dropped; nothing is
 * freed.
 *
 * @param[in] h      The table. Must not be NULL and must have been initialised by
 *                   `fhash_init`.
 * @param[in] key    Pointer to the key bytes to remove. Must not be NULL when `keylen`
 *                   is non-zero.
 * @param[in] keylen Length of the key in bytes.
 * @return Status code:
 *      - 1 if a matching entry was found and removed; `count` is decremented
 *      - 0 if the key was not present; the table is left unchanged
 *
 * @note Never sets errno; the 0 return is "not present", not an error. Does not check
 *       `h` or `key` for NULL: passing a NULL `h`, a NULL `key` with non-zero
 *       `keylen`, or an uninitialised table is undefined behaviour.
 */
extern int fhash_delete(fhash_t *h, void const *key, uint32_t keylen) ;

/**
 * @brief Number of live entries.
 *
 * NULL-safe: returns the table's `count` field, or 0 when `h` is NULL. Always
 * succeeds and never sets errno.
 *
 * @param[in] h The table, or NULL.
 * @return The number of live entries in the table; 0 if `h` is NULL.
 */
extern uint32_t fhash_count(fhash_t const *h) ;

#endif
