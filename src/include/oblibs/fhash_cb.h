/*
 * fhash_cb.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * Option 3: fhash with typed callbacks (the skalibs model).
 *
 * Difference with plain fhash: the table never touches the key's raw bytes. It stores
 * only the caller's value (a pool index) plus the cached hash. To compare or hash, it
 * calls back into caller-supplied functions that operate on TYPED fields:
 *
 *   - hash(key)            : turn a key into a 32-bit hash   (== how avltreen never needs this,
 *                            but a hash table does)
 *   - keyof(value, aux)    : given a stored value, return a pointer to its key   (== skalibs dtok)
 *   - eq(a, b, aux)        : compare two keys for equality                       (== skalibs cmp == 0)
 *
 * Because eq/hash read named fields (sv->dev, sv->ino), padding is never observed: this is
 * representation-independent on every platform, exactly like avltreen + devino_cmp. The price
 * is that each key type needs its three small callbacks, and lookups pay one indirect call.
 *
 * 'aux' is an opaque context passed to keyof/eq (typically the service pool), so a key can be
 * fetched from a stored index without globals — same role as avltreen's aux.
 *
 * Storage model is identical to fhash: caller-provided slot array, zero allocation, power-of-two
 * size with load < 1, tombstone-free backward-shift deletion. A slot stores value+1 so that 0
 * marks "empty" (value 0 is a valid pool index).
 */

#ifndef OBLIBS_FHASH_CB_H
#define OBLIBS_FHASH_CB_H

#include <stdint.h>

/**
 * @brief Hash a key into a 32-bit value.
 *
 * Caller-supplied callback that maps a key to its hash. It is invoked on the
 * lookup key by `fhash_cb_search()`, `fhash_cb_insert()` and `fhash_cb_delete()`
 * before probing the table. The hash it returns is cached verbatim in the slot
 * (`fhash_cb_slot_t::hash`) and reused for probing and for backward-shift
 * deletion, so the same key must always hash to the same value for the life of
 * the entry.
 *
 * @param[in] key Pointer to the key to hash. The library passes through, without
 *                inspection, the `key` argument it received; never NULL unless the
 *                caller passed NULL.
 * @param[in] aux The opaque context registered at `fhash_cb_init()` time.
 * @return The 32-bit hash of `key`. Any 32-bit value is acceptable; only the low
 *         bits (masked by `n - 1`) select the home slot.
 */
typedef uint32_t (*fhash_cb_hash_func)(void const *key, void *aux) ;

/**
 * @brief Resolve a stored value back to a pointer to its key.
 *
 * Caller-supplied callback used to recover a key from a value that is already in
 * the table, so that equality can be tested without the table ever storing the
 * key bytes. It is called during probing in `fhash_cb_search()`,
 * `fhash_cb_insert()` and `fhash_cb_delete()`, with the decoded stored value
 * (slot `v - 1`).
 *
 * @param[in] value A value previously inserted into the table (typically a pool
 *                  index). Always a value the caller itself inserted.
 * @param[in] aux The opaque context registered at `fhash_cb_init()` time.
 * @return Pointer to the key associated with `value`. Must remain valid and stable
 *         for as long as `value` stays in the table; it is passed straight to the
 *         `fhash_cb_eq_func` callback.
 */
typedef void const *(*fhash_cb_keyof_func)(uint32_t value, void *aux) ;

/**
 * @brief Test two keys for equality.
 *
 * Caller-supplied callback that decides whether a probed entry matches the lookup
 * key. It is consulted only after a cached-hash match, so it disambiguates hash
 * collisions. Argument `a` is the lookup key passed by the caller; argument `b` is
 * the key returned by `fhash_cb_keyof_func` for the candidate stored entry.
 *
 * @param[in] a   The lookup key (the `key` argument given to search/insert/delete).
 * @param[in] b   The candidate entry's key, as returned by the `keyof` callback.
 * @param[in] aux The opaque context registered at `fhash_cb_init()` time.
 * @return Truth value of "the keys are equal":
 *      - non-zero if `a` and `b` are the same key (the library treats any non-zero
 *        return as a match and stops probing)
 *      - 0 if they differ (probing continues)
 * @note This is the inverse polarity of a C `memcmp`/`strcmp` comparator: it must
 *       return non-zero (true) on equality, i.e. `cmp(a, b) == 0`.
 */
typedef int (*fhash_cb_eq_func)(void const *a, void const *b, void *aux) ;

/**
 * @struct fhash_cb_slot_t
 * @brief One bucket of the open-addressing table.
 *
 * The table is a flat array of these slots. A slot is either empty (`v == 0`) or
 * occupied. An occupied slot never stores the key itself, only the caller's value
 * (biased by one) and the cached hash of that value's key. The bias by one lets
 * the value 0 (a valid pool index) be stored while reserving the all-zero slot as
 * the "empty" sentinel, so a zero-filled array is a valid empty table.
 *
 * @param v
 * Occupancy and value. 0 means the slot is empty; any other value `k` means the
 * slot holds the caller value `k - 1`.
 *
 * @param hash
 * The cached 32-bit hash of the stored entry's key, as returned by the `hash`
 * callback at insertion time. Used to filter probe candidates before calling `eq`,
 * and as the sole key proxy during backward-shift deletion (the key bytes are
 * never re-read on delete). Meaningful only when `v != 0`.
 */
typedef struct fhash_cb_slot_s fhash_cb_slot_t ;
struct fhash_cb_slot_s
{
    uint32_t v ;       // 0 = empty, otherwise value = v - 1
    uint32_t hash ;    // cached hash of the entry's key
} ;

/**
 * @brief Initialiser for an empty `fhash_cb_slot_t` (empty slot).
 *
 * Expands to `{ 0, 0 }`, marking the slot empty. Useful to statically zero a slot
 * array; `fhash_cb_init()` also zeroes every slot, so this is only needed for
 * static/aggregate initialisation done outside `fhash_cb_init()`.
 */
#define FHASH_CB_SLOT_ZERO { 0, 0 }

/**
 * @struct fhash_cb_t
 * @brief A fixed-capacity, zero-allocation hash set over caller-owned storage.
 *
 * Open-addressing (linear-probing) hash table that maps keys to caller values
 * (pool indices). It never allocates and never copies key bytes: the slot array is
 * supplied by the caller and key access is delegated to three callbacks
 * (`hash`/`keyof`/`eq`). Collisions are resolved by linear probing; deletion uses
 * tombstone-free backward-shift on the cached slot hashes. Capacity is fixed at
 * `fhash_cb_init()` time.
 *
 * Invariants (established by `fhash_cb_init()` and preserved by the operations):
 * `n` is a power of two and `mask == n - 1`; `count` is the number of occupied
 * slots; `fhash_cb_insert()` keeps `count <= n - 1`, so the slot array always contains
 * at least one empty slot and the probe loops always terminate. Sizing with
 * `fhash_cb_sizefor()` is still recommended for probe performance.
 *
 * @param slots
 * Caller-owned array of `n` slots. The table reads and writes this array in place;
 * the caller owns its lifetime and must keep it valid for the life of the table.
 *
 * @param n
 * Number of slots. Always a power of two (enforced at init). One slot is always kept
 * empty, so the usable capacity is `n - 1`: `fhash_cb_insert()` refuses a new key once
 * `count` reaches `n - 1`.
 *
 * @param mask
 * Probe mask, equal to `n - 1`; `hash & mask` selects the home slot.
 *
 * @param count
 * Current number of occupied slots.
 *
 * @param hash
 * The key-hashing callback (see `fhash_cb_hash_func`).
 *
 * @param keyof
 * The value-to-key resolution callback (see `fhash_cb_keyof_func`).
 *
 * @param eq
 * The key-equality callback (see `fhash_cb_eq_func`).
 *
 * @param aux
 * Opaque context handed to all three callbacks (typically the value pool). May be
 * NULL; the library never dereferences it.
 */
typedef struct fhash_cb_s fhash_cb_t ;
struct fhash_cb_s
{
    fhash_cb_slot_t *slots ;
    uint32_t n ;
    uint32_t mask ;
    uint32_t count ;
    fhash_cb_hash_func hash ;
    fhash_cb_keyof_func keyof ;
    fhash_cb_eq_func eq ;
    void *aux ;
} ;

/**
 * @brief Initialiser for an empty, unusable `fhash_cb_t`.
 *
 * Expands to all-zero (`{ 0, ... }`): NULL `slots`, zero `n`/`mask`/`count`, NULL
 * callbacks. This is a placeholder only; a table so initialised has no storage and
 * no callbacks and must not be passed to any operation other than
 * `fhash_cb_init()`. `fhash_cb_count()` on it returns 0.
 */
#define FHASH_CB_ZERO { 0, 0, 0, 0, 0, 0, 0, 0 }

/**
 * @brief Smallest power-of-two slot count that holds @capacity entries comfortably.
 *
 * Computes a target of `capacity + capacity / 2` (a 1.5x headroom so the load
 * factor stays below 1) and returns the smallest power of two strictly greater
 * than that target. The result is at least 2 (for `capacity == 0` or 1 it returns
 * 2). Use the result as the `n` argument to `fhash_cb_init()` to guarantee the
 * table always keeps a spare empty slot.
 *
 * @param[in] capacity Expected number of entries.
 * @return The chosen power-of-two slot count (> the 1.5x target), at least 2. Returns
 *         0 when no `uint32_t` power of two can hold the target
 *         (`capacity + capacity / 2 >= 2^31`); `fhash_cb_init()` then rejects the 0
 *         with EINVAL. Never sets errno.
 *
 * @note The target is computed in 64-bit and the candidate is bounded, so there is no
 *       overflow and the search always terminates: an out-of-range @capacity yields a
 *       clean 0 return, never a wrapped value or a non-terminating loop.
 */
extern uint32_t fhash_cb_sizefor(uint32_t capacity) ;

/**
 * @brief Initialise a table over caller storage with the three typed callbacks.
 *
 * Validates the arguments, then binds @h to the caller's slot array and callbacks,
 * sets `mask = n - 1`, resets `count` to 0, and zeroes every one of the @n slots
 * (so the array need not be pre-zeroed). After this returns 1, @h is ready for
 * search/insert/delete. No memory is allocated; @slots and @aux must outlive @h.
 *
 * @param[out] h     Table to initialise. Must not be NULL.
 * @param[in]  slots Caller-owned array of at least @n slots. Must not be NULL; its
 *                   contents are overwritten with empty slots.
 * @param[in]  n     Number of slots. Must be non-zero and a power of two. To keep a
 *                   spare empty slot, size it with `fhash_cb_sizefor()`.
 * @param[in]  hash  Key-hashing callback. Must not be NULL.
 * @param[in]  keyof Value-to-key resolution callback. Must not be NULL.
 * @param[in]  eq    Key-equality callback. Must not be NULL.
 * @param[in]  aux   Opaque context passed to all three callbacks. May be NULL; it
 *                   is stored as-is and never dereferenced by the library.
 * @return Status code:
 *      - 1 on success; @h is fully initialised and all @n slots are empty
 *      - 0 on failure; errno is set, @h is left untouched
 * @retval 0 errno is set to EINVAL if @h, @slots, @hash, @keyof or @eq is NULL, if
 *           @n is 0, or if @n is not a power of two (`n & (n - 1)` is non-zero).
 *           @aux being NULL is not an error.
 */
extern int fhash_cb_init(fhash_cb_t *h, fhash_cb_slot_t *slots, uint32_t n, fhash_cb_hash_func hash, fhash_cb_keyof_func keyof, fhash_cb_eq_func eq, void *aux) ;

/**
 * @brief Look up a key and, on a hit, report its stored value.
 *
 * Hashes @key via the table's `hash` callback, probes linearly from the home slot,
 * and for each candidate whose cached hash matches calls `eq(key, keyof(value))`
 * to confirm the match. Probing stops at the first empty slot (a miss) or at the
 * confirmed match (a hit). The table is not modified.
 *
 * @param[in]  h     Initialised table. Must not be NULL and must have been set up
 *                   by `fhash_cb_init()`; it is dereferenced unconditionally.
 * @param[in]  key   Key to look up. Passed straight to the `hash` and `eq`
 *                   callbacks, which determine whether NULL is acceptable.
 * @param[out] value Receives the stored value (a value previously inserted) on a
 *                   hit. May be NULL, in which case the hit/miss result is still
 *                   returned but no value is written.
 * @return Status code:
 *      - 1 if the key is present; if @value is non-NULL, `*value` is set to the
 *        stored value
 *      - 0 if the key is absent (clean miss); @value is left unchanged
 * @note Never fails and never sets errno; both returns are normal results.
 * @note If the table has no empty slot (`count == n`), the probe loop cannot
 *       terminate on a miss. Keep at least one slot free (see `fhash_cb_sizefor()`).
 */
extern int fhash_cb_search(fhash_cb_t const *h, void const *key, uint32_t *value) ;

/**
 * @brief Insert a key/value pair, or update the value of an existing key.
 *
 * First refuses if the table is full. Otherwise hashes @key and probes linearly:
 * if an existing entry with the same key is found (cached-hash match confirmed by
 * `eq`), its stored value is overwritten with @value and `count` is unchanged; if
 * the probe reaches an empty slot, the pair is written there, the slot's cached
 * hash is set, and `count` is incremented.
 *
 * @param[in,out] h     Initialised table. Must not be NULL; dereferenced
 *                      unconditionally.
 * @param[in]     key   Key to insert or update. Passed to the `hash`/`eq`
 *                      callbacks (and `keyof` for existing entries).
 * @param[in]     value Caller value to associate with @key (e.g. a pool index).
 *                      Stored internally as `value + 1`; any `uint32_t` is valid.
 * @return Status code:
 *      - 1 on success: the pair was inserted (new key) or the value was updated
 *        (existing key)
 *      - 0 on failure; errno is set
 * @retval 0 errno is set to ENOSPC when inserting a new key would consume the last free
 *           slot (`count >= n - 1`); the table holds at most `n - 1` entries so a free
 *           slot is always reserved for probe termination. Updating an already-present
 *           key always succeeds.
 * @note The reserved empty slot guarantees that search/insert/delete of an absent key
 *       always terminate, whatever the caller does. `fhash_cb_sizefor()` is still
 *       recommended to keep the load low for performance.
 */
extern int fhash_cb_insert(fhash_cb_t *h, void const *key, uint32_t value) ;

/**
 * @brief Remove a key from the table using tombstone-free backward-shift.
 *
 * Hashes @key and probes linearly for it (cached-hash match confirmed by `eq`). If
 * found, the slot is cleared and the run of following entries is back-shifted to
 * preserve probe correctness, then `count` is decremented. The back-shift works
 * purely on cached slot hashes; the key callbacks are not invoked for the shifted
 * entries.
 *
 * @param[in,out] h   Initialised table. Must not be NULL; dereferenced
 *                    unconditionally.
 * @param[in]     key Key to remove. Passed to the `hash`/`eq` callbacks (and
 *                    `keyof` for probed entries).
 * @return Status code:
 *      - 1 if the key was present and has been removed (`count` decremented)
 *      - 0 if the key was absent; the table is unchanged
 * @note Never fails and never sets errno; both returns are normal results.
 * @note If the table has no empty slot (`count == n`), the search phase cannot
 *       terminate on an absent key. Keep at least one slot free.
 */
extern int fhash_cb_delete(fhash_cb_t *h, void const *key) ;

/**
 * @brief Report the number of entries currently in the table.
 *
 * @param[in] h Table to query. NULL is accepted.
 * @return The number of occupied slots (`h->count`), or 0 if @h is NULL. Always
 *         succeeds and never sets errno.
 */
extern uint32_t fhash_cb_count(fhash_cb_t const *h) ;

#endif
