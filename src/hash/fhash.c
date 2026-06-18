/*
 * fhash.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * Fixed-capacity, zero-malloc, open-addressing hash table. See fhash.h.
 */

#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <oblibs/fhash.h>

/* FNV-1a, 32-bit. Cheap, no state, good dispersion on small integer keys
   (pids) and small structs (devino) alike. */
static uint32_t fhash_hash(void const *key, uint32_t keylen)
{
    uint32_t h = 2166136261u ;
    unsigned char const *p = key ;

    for (uint32_t i = 0 ; i < keylen ; i++) {
        h ^= p[i] ;
        h *= 16777619u ;
    }
    return h ;
}

static int slot_match(fhash_slot_t const *s, void const *key, uint32_t keylen, uint32_t hash)
{
    return s->hash == hash && s->keylen == keylen && !memcmp(s->key, key, keylen) ;
}

uint32_t fhash_sizefor(uint32_t capacity)
{
    uint64_t need = (uint64_t)capacity + (capacity >> 1) ;   // ~1.5x -> load stays <= ~0.67
    uint32_t n = 2 ;

    if (need >= 0x80000000u)   // no uint32 power of two (max 2^31) exceeds need: signal failure
        return 0 ;
    while (n <= need)
        n <<= 1 ;
    return n ;
}

int fhash_init(fhash_t *h, fhash_slot_t *slots, uint32_t n)
{
    if (!h || !slots || !n || (n & (n - 1)))
        return (errno = EINVAL, 0) ;

    h->slots = slots ;
    h->n = n ;
    h->mask = n - 1 ;
    h->count = 0 ;
    memset(slots, 0, (size_t)n * sizeof(fhash_slot_t)) ;
    return 1 ;
}

int fhash_search(fhash_t const *h, void const *key, uint32_t keylen, uint32_t *value)
{
    uint32_t hash = fhash_hash(key, keylen) ;
    uint32_t i = hash & h->mask ;

    /* Walks until the key or an empty slot. Terminates because the load is < 1,
       so at least one empty slot always exists in the probe path. */
    while (h->slots[i].key) {
        if (slot_match(&h->slots[i], key, keylen, hash)) {
            if (value) *value = h->slots[i].value ;
            return 1 ;
        }
        i = (i + 1) & h->mask ;
    }
    return 0 ;
}

int fhash_insert(fhash_t *h, void const *key, uint32_t keylen, uint32_t value)
{
    uint32_t hash = fhash_hash(key, keylen) ;
    uint32_t i ;

    /* Probe first. The reserve-one-slot rule below keeps at least one empty slot at all
       times, so this loop always terminates (it finds the key or an empty slot). */
    for (i = hash & h->mask ; h->slots[i].key ; i = (i + 1) & h->mask) {
        if (slot_match(&h->slots[i], key, keylen, hash)) {
            h->slots[i].value = value ;  // already present: update in place
            return 1 ;
        }
    }

    /* New key: keep one slot permanently empty so search/delete probes can never spin
       on a full table. This makes load < 1 a structural invariant, not a precondition. */
    if (h->count >= h->n - 1)
        return (errno = ENOSPC, 0) ;

    h->slots[i].key = key ;
    h->slots[i].keylen = keylen ;
    h->slots[i].hash = hash ;
    h->slots[i].value = value ;
    h->count++ ;
    return 1 ;
}

int fhash_delete(fhash_t *h, void const *key, uint32_t keylen)
{
    uint32_t hash = fhash_hash(key, keylen) ;
    uint32_t i = hash & h->mask ;
    uint32_t j ;

    while (h->slots[i].key) {
        if (slot_match(&h->slots[i], key, keylen, hash))
            break ;
        i = (i + 1) & h->mask ;
    }
    if (!h->slots[i].key)
        return 0 ; // not present

    /** Backward-shift (Knuth 6.4R). 'i' is the hole. Scan forward over the
     * contiguous cluster; for each element at 'j' whose home slot is such that the
     * hole lies on its probe path, pull it back into the hole, then make 'j' the new
     * hole and continue. Stop at the first empty slot. This leaves the table exactly
     * as if 'key' had never been inserted — no tombstone, no degradation over time. */
    j = i ;
    for (;;) {
        h->slots[i].key = 0 ;
        for (;;) {
            uint32_t home ;
            j = (j + 1) & h->mask ;
            if (!h->slots[j].key) { // end of the cluster: done
                h->count-- ;
                return 1 ;
            }
            home = h->slots[j].hash & h->mask ;
            /* Keep scanning while 'home' is cyclically inside (i, j]: moving such an
               element back to 'i' would place it before its home and break lookups. */
            if (!((i <= j) ? (home > i && home <= j)
                           : (home > i || home <= j)))
                break ;
        }
        h->slots[i] = h->slots[j] ; // relocate; the hole moves to 'j'
        i = j ;
    }
}

uint32_t fhash_count(fhash_t const *h)
{
    return h ? h->count : 0 ;
}
