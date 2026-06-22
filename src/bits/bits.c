/*
 * bits.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>

#include <oblibs/bits.h>

bitset32_t bitset32_init(uint32_t len)
{
    bitset32_t bs = BITSET32_ZERO ;
    uint32_t rsize = BITSET32_WORDS_SIZE(len) ;

    if (rsize > BITSET32_ARRAY_SIZE)
        return bs ;

    bs.size = rsize ;
    return bs ;
}

void bitset32_set(bitset32_t *bitset, uint32_t index)
{
    bitset->bits[index / UINT32_BITS] |= (1U << (index % UINT32_BITS)) ;
    bitset->count++ ;
}

void bitset32_clear(bitset32_t *bitset, uint32_t index)
{
    bitset->bits[index / UINT32_BITS] &= ~(1U << (index % UINT32_BITS)) ;
    if (bitset->count)
        bitset->count-- ;
}

int bitset32_isvalid(const bitset32_t *bitset, uint32_t index)
{
    return bitset->bits[index / UINT32_BITS] & (1U << (index % UINT32_BITS)) ;
}

void bitset32_free(bitset32_t *bitset)
{
    if (bitset && bitset->size > 0)
        bitset->size = 0 ;
}

bitset8_t bitset8_init(uint8_t len)
{
    bitset8_t bs = BITSET8_ZERO ;
    uint8_t rsize = BITSET8_WORDS_SIZE(len) ;

    if (rsize > BITSET8_ARRAY_SIZE)
        return bs ;

    bs.size = rsize ;
    return bs ;
}

void bitset8_set(bitset8_t *bitset, uint8_t index)
{
    bitset->bits[index / UINT8_BITS] |= (1U << (index % UINT8_BITS)) ;
    bitset->count++ ;
}

void bitset8_clear(bitset8_t *bitset, uint8_t index)
{
    bitset->bits[index / UINT8_BITS] &= ~(1U << (index % UINT8_BITS)) ;
    if (bitset->count)
        bitset->count-- ;
}

int bitset8_isvalid(const bitset8_t *bitset, uint8_t index)
{
    return bitset->bits[index / UINT8_BITS] & (1U << (index % UINT8_BITS)) ;
}

void bitset8_free(bitset8_t *bitset)
{
    if (bitset && bitset->size > 0)
        bitset->size = 0 ;
}
