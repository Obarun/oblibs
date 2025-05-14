/*
 * bits.h
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

 #ifndef OB_BITS_H
 #define OB_BITS_H

 #include <stdint.h>

 #define UINT32_BITS 32
 #define UINT32_BITS_MAX 1024
 #define BITSET_WORDS_SIZE(n) ((n + 31) / 32)
 #define BITSET_ARRAY_SIZE ((UINT32_BITS_MAX + 31) / UINT32_BITS)  // Round up to nearest multiple of 32

 /**
  * @struct bitset_t
  * @brief Represents a set of bits.
  *
  * This bitset is used to mark the edges of one vertex on other vertexes,
  * or the vertexes that depend on it.
  *
  * @param bits
  * A bits array.
  *
  * @param size
  * The size of the bitset, representing the number of bits.
  */
 typedef struct bitset_s bitset_t ;
 struct bitset_s {
     uint32_t bits[BITSET_ARRAY_SIZE] ;
     uint32_t size ;
 } ;

 #define BITSET_ZERO { {0}, 0 }

 /**
  * @brief Creates an empty bitset of the specified length.
  *
  * This function initializes a bitset structure with all bits set to 0. The
  * number of bits is determined by the input length, rounded up to the nearest
  * multiple of the size of an unsigned integer.
  *
  * @param len
  * The number of bits required in the bitset.
  *
  * @return
  * A `bitset_t` structure with all bits set to 0 and a valid size.
  * If the required size exceeds the maximum allowed (`UINT32_BITS_MAX / UINT32_BITS`),
  * the size of the returned bitset is set to 0, indicating failure.
  *
  * @note
  * - The function clears the `bits` array in the bitset structure to ensure all
  *   bits are initialized to 0.
  * - The size of the bitset is stored in `bitset_t::size`, representing the number
  *   of valid entries in the `bits` array.
  */
 extern bitset_t bitset_create_empty(uint32_t len) ;

 /**
  * @brief Sets a specific bit in the bitset to 1.
  *
  * This function modifies the bitset to mark a specific bit as active (set to 1),
  * based on the provided index.
  *
  * @param bitset
  * Pointer to the bitset to modify.
  *
  * @param index
  * The index of the bit to set.
  *
  * @note
  * - The function does not perform bounds checking on the `index`. Ensure that the
  *   index is within the range of the bitset's size.
  */
 extern void bitset_set(bitset_t *bitset, uint32_t index) ;

 /**
  * @brief Clears a specific bit in the bitset, setting it to 0.
  *
  * This function modifies the bitset to mark a specific bit as inactive (set to 0),
  * based on the provided index.
  *
  * @param bitset
  * Pointer to the bitset to modify.
  *
  * @param index
  * The index of the bit to clear.
  *
  * @note
  * - The function does not perform bounds checking on the `index`. Ensure that the
  *   index is within the range of the bitset's size.
  */
 extern void bitset_clear(bitset_t *bitset, uint32_t index) ;

 /**
  * @brief Checks if a specific bit in the bitset is set to 1.
  *
  * This function checks whether the bit at the given index in the bitset is active
  * (set to 1).
  *
  * @param bitset
  * Pointer to the bitset to check.
  *
  * @param index
  * The index of the bit to check.
  *
  * @return
  * A non-zero value if the bit is set to 1, or 0 if the bit is 0.
  *
  * @note
  * - The function does not perform bounds checking on the `index`. Ensure that the
  *   index is within the range of the bitset's size.
  */
 extern int bitset_isvalid(const bitset_t *bitset, uint32_t index) ;

 /**
  * @brief Frees a bitset structure.
  *
  * This function clears the contents of the bitset and resets its size.
  *
  * @param bitset
  * Pointer to the bitset structure to be freed.
  *
  * @note
  * - If the bitset is NULL or its size is 0, the function does nothing.
  * - The memory for the bitset structure itself is not freed.
  */
 extern void bitset_free(bitset_t *bitset) ;

 #endif