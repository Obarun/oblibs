/*
 * fhash_cb.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * Option 3: callback-based fhash (skalibs model). See fhash_cb.h.
 *
 * Compare this file line for line with fhash.c: the structure is identical. The ONLY
 * changes are that fhash_hash()/memcmp are replaced by h->hash()/h->eq() on keys fetched
 * through h->keyof(). The deletion's backward-shift is byte-for-byte the same (it uses the
 * cached slot hash, never the key).
 */

#include <errno.h>
#include <stdint.h>

#include <oblibs/fhash_cb.h>

uint32_t fhash_cb_sizefor(uint32_t capacity)
{
    uint64_t need = (uint64_t)capacity + (capacity >> 1) ;
    uint32_t n = 2 ;

    if (need >= 0x80000000u)   // no uint32 power of two (max 2^31) exceeds need: signal failure
        return 0 ;
    while (n <= need)
        n <<= 1 ;
    return n ;
}

int fhash_cb_init(fhash_cb_t *h, fhash_cb_slot_t *slots, uint32_t n, fhash_cb_hash_func hash, fhash_cb_keyof_func keyof, fhash_cb_eq_func eq, void *aux)
{
    if (!h || !slots || !n || (n & (n - 1)) || !hash || !keyof || !eq)
        return (errno = EINVAL, 0) ;

    h->slots = slots ;
    h->n = n ;
    h->mask = n - 1 ;
    h->count = 0 ;
    h->hash = hash ;
    h->keyof = keyof ;
    h->eq = eq ;
    h->aux = aux ;
    for (uint32_t i = 0 ; i < n ; i++) { slots[i].v = 0 ; slots[i].hash = 0 ; }
    return 1 ;
}

int fhash_cb_search(fhash_cb_t const *h, void const *key, uint32_t *value)
{
    uint32_t hash = h->hash(key, h->aux) ;
    uint32_t i ;

    for (i = hash & h->mask ; h->slots[i].v ; i = (i + 1) & h->mask) {
        if (h->slots[i].hash == hash) {
            uint32_t val = h->slots[i].v - 1 ;
            if (h->eq(key, h->keyof(val, h->aux), h->aux)) {
                if (value) *value = val ;
                return 1 ;
            }
        }
    }
    return 0 ;
}

int fhash_cb_insert(fhash_cb_t *h, void const *key, uint32_t value)
{
    uint32_t hash = h->hash(key, h->aux) ;
    uint32_t i ;

    /* Probe first. The reserve-one-slot rule below keeps at least one empty slot at all
       times, so this loop always terminates (it finds the key or an empty slot). */
    for (i = hash & h->mask ; h->slots[i].v ; i = (i + 1) & h->mask) {
        if (h->slots[i].hash == hash
         && h->eq(key, h->keyof(h->slots[i].v - 1, h->aux), h->aux)) {
            h->slots[i].v = value + 1 ;   // already present: update
            return 1 ;
        }
    }

    /* New key: keep one slot permanently empty so search/delete probes can never spin
       on a full table. This makes load < 1 a structural invariant, not a precondition. */
    if (h->count >= h->n - 1)
        return (errno = ENOSPC, 0) ;

    h->slots[i].v = value + 1 ;
    h->slots[i].hash = hash ;
    h->count++ ;
    return 1 ;
}

int fhash_cb_delete(fhash_cb_t *h, void const *key)
{
    uint32_t hash = h->hash(key, h->aux) ;
    uint32_t i = hash & h->mask ;
    uint32_t j ;

    while (h->slots[i].v) {
        if (h->slots[i].hash == hash
         && h->eq(key, h->keyof(h->slots[i].v - 1, h->aux), h->aux))
            break ;
        i = (i + 1) & h->mask ;
    }
    if (!h->slots[i].v)
        return 0 ;

    /* Backward-shift, identical to fhash.c: works on cached hashes, never on keys. */
    j = i ;
    for (;;) {
        h->slots[i].v = 0 ;
        for (;;) {
            uint32_t home ;
            j = (j + 1) & h->mask ;
            if (!h->slots[j].v) {
                h->count-- ;
                return 1 ;
            }
            home = h->slots[j].hash & h->mask ;
            if (!((i <= j) ? (home > i && home <= j)
                           : (home > i || home <= j)))
                break ;
        }
        h->slots[i] = h->slots[j] ;
        i = j ;
    }
}

uint32_t fhash_cb_count(fhash_cb_t const *h)
{
    return h ? h->count : 0 ;
}
