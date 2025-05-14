/*
 * bits.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

 #include <stdint.h>
 #include <string.h>

 #include <oblibs/bits.h>

 bitset_t bitset_create_empty(uint32_t len)
 {
     bitset_t bs = BITSET_ZERO ;
     uint32_t rsize = BITSET_WORDS_SIZE(len) ;

     if (rsize > BITSET_ARRAY_SIZE)
         return bs ;

     bs.size = rsize ;
     return bs ;
 }

 void bitset_set(bitset_t *bitset, uint32_t index)
 {
     if (bitset && index < bitset->size)
         bitset->bits[index / UINT32_BITS] |= (1U << (index % UINT32_BITS)) ;
 }

 void bitset_clear(bitset_t *bitset, uint32_t index)
 {
     if (bitset && index < bitset->size)
         bitset->bits[index / UINT32_BITS] &= ~(1U << (index % UINT32_BITS)) ;
 }

 int bitset_isvalid(const bitset_t *bitset, uint32_t index)
 {
     if (bitset && index < bitset->size)
         return bitset->bits[index / UINT32_BITS] & (1U << (index % UINT32_BITS)) ;

     return 0 ;
 }

 void bitset_free(bitset_t *bitset)
 {
     if (bitset && bitset->size > 0)
         bitset->size = 0 ;
 }
