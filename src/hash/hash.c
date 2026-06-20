/*
 * hash.c
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
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <oblibs/hash.h>

#define HASH_MIN_BUCKETS 8

static size_t hash_pow2_ceil(size_t n)
{
    size_t p = HASH_MIN_BUCKETS ;
    while (p < n)
        p <<= 1 ;
    return p ;
}

/* FNV-1a, 32-bit. */
static uint32_t hash_key(const void *key, size_t keylen)
{
    const unsigned char *p = (const unsigned char *)key ;
    uint32_t h = 2166136261u ;
    for (size_t i = 0 ; i < keylen ; i++) {
        h ^= p[i] ;
        h *= 16777619u ;
    }
    return h ;
}

int hash_init(hash_t *h, size_t capacity, size_t node_off)
{
    if (!h)
        return (errno = EINVAL, 0) ;

    size_t nbuckets = hash_pow2_ceil(capacity) ;

    hash_node_t **buckets = (hash_node_t **)calloc(nbuckets, sizeof(hash_node_t *)) ;
    if (!buckets)
        return (errno = ENOMEM, 0) ;

    h->buckets = buckets ;
    h->ahead = NULL ;
    h->atail = NULL ;
    h->nbuckets = nbuckets ;
    h->count = 0 ;
    h->node_off = node_off ;

    return 1 ;
}

void hash_free(hash_t *h)
{
    if (!h)
        return ;

    free(h->buckets) ;
    h->buckets = NULL ;
    h->ahead = NULL ;
    h->atail = NULL ;
    h->nbuckets = 0 ;
    h->count = 0 ;
    h->node_off = 0 ;
}

void *hash_find(hash_t *h, const void *key, size_t keylen)
{
    if (!h || !h->buckets)
        return (errno = EINVAL, NULL) ;

    if (!h->count)
        return NULL ;

    uint32_t hashv = hash_key(key, keylen) ;
    size_t bkt = hashv & (h->nbuckets - 1) ;

    for (hash_node_t *n = h->buckets[bkt] ; n ; n = n->hnext) {
        if (n->hashv == hashv && n->keylen == keylen
            && memcmp(n->key, key, keylen) == 0)
            return HASH_ELT_OF(h, n) ;
    }

    return NULL ;
}

static int hash_grow(hash_t *h)
{
    size_t newn = h->nbuckets << 1 ;

    hash_node_t **nb = (hash_node_t **)calloc(newn, sizeof(hash_node_t *)) ;
    if (!nb)
        return (errno = ENOMEM, 0) ;

    /* Re-bucket every node. Walking the insertion-order list leaves anext/aprev,
     * hence iteration order, untouched. */
    for (hash_node_t *n = h->ahead ; n ; n = n->anext) {
        size_t bkt = n->hashv & (newn - 1) ;
        n->hprev = NULL ;
        n->hnext = nb[bkt] ;
        if (nb[bkt])
            nb[bkt]->hprev = n ;
        nb[bkt] = n ;
    }

    free(h->buckets) ;
    h->buckets = nb ;
    h->nbuckets = newn ;

    return 1 ;
}

int hash_add(hash_t *h, const void *key, size_t keylen, void *elem)
{
    if (!h || !h->buckets || !key || !keylen || !elem)
        return (errno = EINVAL, 0) ;

    if (h->count + 1 > h->nbuckets * HASH_MAX_LOAD) {
        if (!hash_grow(h))
            return 0 ;
    }

    uint32_t hashv = hash_key(key, keylen) ;
    size_t bkt = hashv & (h->nbuckets - 1) ;

    hash_node_t *node = HASH_NODE_OF(h, elem) ;
    node->key = key ;
    node->keylen = (uint32_t)keylen ;
    node->hashv = hashv ;

    // Insert at the head of the bucket chain.
    node->hprev = NULL ;
    node->hnext = h->buckets[bkt] ;
    if (h->buckets[bkt])
        h->buckets[bkt]->hprev = node ;
    h->buckets[bkt] = node ;

    // Append to the insertion-order list.
    node->anext = NULL ;
    node->aprev = h->atail ;
    if (h->atail)
        h->atail->anext = node ;
    else
        h->ahead = node ;
    h->atail = node ;

    h->count++ ;

    return 1 ;
}

void hash_del(hash_t *h, void *elem)
{
    if (!h || !h->buckets || !elem)
        return ;

    hash_node_t *node = HASH_NODE_OF(h, elem) ;

    // Unlink from the bucket chain.
    if (node->hprev)
        node->hprev->hnext = node->hnext ;
    else
        h->buckets[node->hashv & (h->nbuckets - 1)] = node->hnext ;
    if (node->hnext)
        node->hnext->hprev = node->hprev ;

    // Unlink from the insertion-order list.
    if (node->aprev)
        node->aprev->anext = node->anext ;
    else
        h->ahead = node->anext ;
    if (node->anext)
        node->anext->aprev = node->aprev ;
    else
        h->atail = node->aprev ;

    node->hnext = node->hprev = node->anext = node->aprev = NULL ;
    node->key = NULL ;

    h->count-- ;
}

size_t hash_count(const hash_t *h)
{
    return h ? h->count : 0 ;
}
