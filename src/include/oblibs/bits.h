/*
 * bits.h
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

/*
 * @brief Fixed-capacity bitset utilities.
 *
 * This module provides two parallel families of fixed-capacity bitsets backed
 * by an inline array embedded in the structure: a 32-bit-word family
 * (`bitset32_t`, capacity `UINT32_BITS_MAX` bits) and an 8-bit-word family
 * (`bitset8_t`, capacity `UINT8_BITS_MAX` bits). No heap allocation is ever
 * performed: a bitset is a plain value type that the caller stores by value or
 * embeds in another structure.
 *
 * Both families expose the same operations: an `_init` constructor that returns
 * a zeroed bitset by value, and `_set` / `_clear` / `_isvalid` accessors that
 * take the index of a single bit. None of the accessors perform bounds
 * checking and none of the functions set `errno`; the `size` field is the sole
 * validity sentinel (`size == 0` means the bitset is empty or was rejected at
 * construction). The `_free` function exists only to invalidate a bitset by
 * resetting that sentinel; it never releases memory.
 */

#ifndef OB_BITS_H
#define OB_BITS_H

#include <stdint.h>

/**
 * @brief Number of bits in one `uint32_t` storage word.
 *
 * Used as the word width for the `bitset32_t` family: bit `i` lives in word
 * `i / UINT32_BITS` at position `i % UINT32_BITS`.
 */
#define UINT32_BITS 32

/**
 * @brief Maximum number of bits a `bitset32_t` can hold.
 *
 * Determines the inline storage capacity of `bitset32_t`. `bitset32_init()`
 * rejects any requested length whose word count would exceed
 * `BITSET32_ARRAY_SIZE` (i.e. `UINT32_BITS_MAX / UINT32_BITS` words).
 */
#define UINT32_BITS_MAX 1024

/**
 * @brief Number of `uint32_t` words needed to hold @p n bits.
 *
 * Computes `ceil(n / UINT32_BITS)`: the count of 32-bit words required to store
 * @p n bits. Used by `bitset32_init()` to derive the `size` field from a
 * requested bit count.
 *
 * @param n
 * The number of bits. With `n == 0` the macro evaluates to 0.
 *
 * @return The number of `uint32_t` words required to store @p n bits.
 *
 * @note This is a textual macro: @p n is evaluated once but is not
 *       parenthesised internally, so pass a simple expression.
 */
#define BITSET32_WORDS_SIZE(n) (((n) + (UINT32_BITS - 1)) / UINT32_BITS)

/**
 * @brief Length, in `uint32_t` words, of the inline `bits` array.
 *
 * Equals `ceil(UINT32_BITS_MAX / UINT32_BITS)`, the number of 32-bit words
 * dimensioning `bitset32_s::bits`. It is also the upper bound that
 * `bitset32_init()` enforces on the requested word count.
 */
#define BITSET32_ARRAY_SIZE ((UINT32_BITS_MAX + (UINT32_BITS - 1)) / UINT32_BITS)  // Round up to nearest multiple of 32

/**
 * @struct bitset32_s
 * @brief A fixed-capacity bitset backed by an inline array of `uint32_t` words.
 *
 * Stores up to `UINT32_BITS_MAX` bits in an inline array, with no heap
 * allocation. The structure is a value type: it is created by
 * `bitset32_init()`, copied and passed by value or by pointer, and never owns
 * any external resource. It is typically used to mark the edges of one vertex
 * onto other vertexes, or the vertexes that depend on it.
 *
 * @param bits
 * The inline storage array of `BITSET32_ARRAY_SIZE` words. Bit `i` is held in
 * `bits[i / UINT32_BITS]` at position `i % UINT32_BITS`. Zero-initialised by
 * `bitset32_init()`.
 *
 * @param size
 * The number of `uint32_t` words currently in use, as computed by
 * `BITSET32_WORDS_SIZE()` from the length requested at construction. A value of
 * 0 is the validity sentinel: it means the bitset is empty, was constructed
 * from `len == 0`, was rejected by `bitset32_init()` for exceeding capacity, or
 * was invalidated by `bitset32_free()`. This field is NOT updated by `_set`,
 * `_clear` or `_isvalid`.
 *
 * @param count
 * A running tally maintained by `bitset32_set()` (incremented on every call)
 * and `bitset32_clear()` (decremented on every call, but never below 0). It is
 * NOT a count of distinct set bits: setting an already-set bit still
 * increments it, and clearing an already-clear bit still decrements it (down to
 * the 0 floor). It therefore equals the number of distinct set bits only when
 * the caller never repeats a `_set` on a set bit nor a `_clear` on a clear bit.
 */
struct bitset32_s
{
    uint32_t bits[BITSET32_ARRAY_SIZE] ;
    uint32_t size ;
    uint32_t count ;
} ;
typedef struct bitset32_s bitset32_t ;

/**
 * @brief Static initialiser for a zeroed (empty, invalid) `bitset32_t`.
 *
 * Expands to a brace initialiser that clears the `bits` array and sets both
 * `size` and `count` to 0. The resulting bitset has `size == 0`, i.e. it is in
 * the empty/invalid state until populated by `bitset32_init()`.
 */
#define BITSET32_ZERO { {0}, 0, 0 }

/**
 * @brief Construct a zeroed bitset sized to hold @p len bits.
 *
 * Returns, by value, a `bitset32_t` whose `bits` array is entirely zero and
 * whose `count` is 0. The `size` field is set to `BITSET32_WORDS_SIZE(len)`,
 * the number of `uint32_t` words needed to hold @p len bits, unless that word
 * count exceeds `BITSET32_ARRAY_SIZE`, in which case `size` is left at 0. No
 * allocation is performed and `errno` is never modified.
 *
 * @param[in] len
 * The requested number of bits. `len == 0` yields a word count of 0 and is
 * therefore indistinguishable from the rejection case: the returned bitset has
 * `size == 0`.
 *
 * @return A `bitset32_t` value:
 *      - on success, `bits` all 0, `count == 0`, and
 *        `size == BITSET32_WORDS_SIZE(len)` (a value in `1 .. BITSET32_ARRAY_SIZE`)
 *      - on rejection (`BITSET32_WORDS_SIZE(len) > BITSET32_ARRAY_SIZE`) or when
 *        `len == 0`, the all-zero `BITSET32_ZERO` value, identified by
 *        `size == 0`
 *
 * @note This function does not report failure through `errno`; the only failure
 *       signal is `size == 0` in the returned value. Capacity above
 *       `UINT32_BITS_MAX` bits (`BITSET32_ARRAY_SIZE` words) cannot be requested.
 */
extern bitset32_t bitset32_init(uint32_t len) ;

/**
 * @brief Set the bit at @p index to 1 and bump the running tally.
 *
 * Sets bit @p index in `@bitset->bits` (word `index / UINT32_BITS`, position
 * `index % UINT32_BITS`) and unconditionally increments `@bitset->count`.
 *
 * @param[in,out] bitset
 * Pointer to the bitset to modify. Must not be NULL; the function dereferences
 * it without a NULL check.
 *
 * @param[in] index
 * The bit to set.
 *
 * @note No bounds checking is performed: an @p index of `size * UINT32_BITS` or
 *       greater writes outside the in-use words, and an @p index of
 *       `BITSET32_ARRAY_SIZE * UINT32_BITS` or greater writes out of bounds of
 *       the `bits` array (undefined behaviour). The caller must keep @p index in
 *       range.
 * @note `count` is incremented on every call, including when @p index was
 *       already set; it is a call tally, not a population count. See
 *       `bitset32_s::count`.
 */
extern void bitset32_set(bitset32_t *bitset, uint32_t index) ;

/**
 * @brief Clear the bit at @p index to 0 and decrement the running tally.
 *
 * Clears bit @p index in `@bitset->bits` (word `index / UINT32_BITS`, position
 * `index % UINT32_BITS`) and decrements `@bitset->count`, but only when `count`
 * is non-zero, so `count` never underflows below 0.
 *
 * @param[in,out] bitset
 * Pointer to the bitset to modify. Must not be NULL; the function dereferences
 * it without a NULL check.
 *
 * @param[in] index
 * The bit to clear.
 *
 * @note No bounds checking is performed: an @p index at or beyond
 *       `BITSET32_ARRAY_SIZE * UINT32_BITS` accesses the `bits` array out of
 *       bounds (undefined behaviour). The caller must keep @p index in range.
 * @note `count` is decremented on every call where `count > 0`, including when
 *       @p index was already clear; it is a call tally, not a population count.
 *       See `bitset32_s::count`.
 */
extern void bitset32_clear(bitset32_t *bitset, uint32_t index) ;

/**
 * @brief Test whether the bit at @p index is set.
 *
 * Returns the result of masking word `index / UINT32_BITS` of `@bitset->bits`
 * with `1U << (index % UINT32_BITS)`.
 *
 * @param[in] bitset
 * Pointer to the bitset to read. Must not be NULL; the function dereferences it
 * without a NULL check.
 *
 * @param[in] index
 * The bit to test.
 *
 * @return The masked word value:
 *      - non-zero (specifically `1 << (index % UINT32_BITS)`, which is negative
 *        when the conversion of `1U << 31` to `int` is implementation-defined)
 *        when the bit is set
 *      - 0 when the bit is clear
 *
 * @note Callers should treat the result as a boolean (compare against 0); the
 *       exact non-zero value carries no meaning beyond "set".
 * @note No bounds checking is performed: an @p index at or beyond
 *       `BITSET32_ARRAY_SIZE * UINT32_BITS` reads the `bits` array out of bounds
 *       (undefined behaviour). The caller must keep @p index in range.
 */
extern int bitset32_isvalid(const bitset32_t *bitset, uint32_t index) ;

/**
 * @brief Invalidate a bitset by resetting its size sentinel.
 *
 * If @p bitset is non-NULL and its `size` is greater than 0, sets `size` to 0,
 * marking the bitset empty/invalid. The `bits` array and the `count` field are
 * left untouched, and no memory is released (the bitset owns none).
 *
 * @param[in,out] bitset
 * Pointer to the bitset to invalidate. May be NULL, in which case the function
 * does nothing.
 *
 * @note Despite the name, this performs no deallocation: a `bitset32_t` is a
 *       value type with inline storage. The function only flips the `size`
 *       sentinel to 0; it does NOT clear the `bits` array and does NOT reset
 *       `count`. A bitset that already has `size == 0` is left unchanged.
 */
extern void bitset32_free(bitset32_t *bitset) ;

/**
 * @brief Number of bits in one `uint8_t` storage word.
 *
 * Used as the word width for the `bitset8_t` family: bit `i` lives in word
 * `i / UINT8_BITS` at position `i % UINT8_BITS`.
 */
#define UINT8_BITS 8

/**
 * @brief Maximum number of bits a `bitset8_t` can hold.
 *
 * Determines the inline storage capacity of `bitset8_t`. `bitset8_init()`
 * rejects any requested length whose word count would exceed
 * `BITSET8_ARRAY_SIZE` (i.e. `UINT8_BITS_MAX / UINT8_BITS` words).
 */
#define UINT8_BITS_MAX 256

/**
 * @brief Number of `uint8_t` words needed to hold @p n bits.
 *
 * Computes `ceil(n / UINT8_BITS)`: the count of 8-bit words required to store
 * @p n bits. Used by `bitset8_init()` to derive the `size` field from a
 * requested bit count.
 *
 * @param n
 * The number of bits. With `n == 0` the macro evaluates to 0.
 *
 * @return The number of `uint8_t` words required to store @p n bits.
 *
 * @note This is a textual macro: @p n is evaluated once but is not
 *       parenthesised internally, so pass a simple expression.
 */
#define BITSET8_WORDS_SIZE(n) (((n) + (UINT8_BITS - 1)) / UINT8_BITS)

/**
 * @brief Length, in `uint8_t` words, of the inline `bits` array.
 *
 * Equals `ceil(UINT8_BITS_MAX / UINT8_BITS)`, the number of 8-bit words
 * dimensioning `bitset8_s::bits`. It is also the upper bound that
 * `bitset8_init()` enforces on the requested word count.
 *
 * @note This evaluates to 32. Because `bitset8_init()` takes a `uint8_t` length
 *       (0..255), the largest word count it can compute is
 *       `BITSET8_WORDS_SIZE(255) == 32`, exactly `BITSET8_ARRAY_SIZE`; the
 *       capacity-rejection branch is therefore never reachable through the
 *       public API.
 */
#define BITSET8_ARRAY_SIZE ((UINT8_BITS_MAX + (UINT8_BITS - 1)) / UINT8_BITS)  // Round up to nearest multiple of 8

/**
 * @struct bitset8_s
 * @brief A fixed-capacity bitset backed by an inline array of `uint8_t` words.
 *
 * Stores up to `UINT8_BITS_MAX` bits in an inline array, with no heap
 * allocation. The structure is a value type: it is created by `bitset8_init()`,
 * copied and passed by value or by pointer, and never owns any external
 * resource. It is the 8-bit-word counterpart of `bitset32_s`.
 *
 * @param bits
 * The inline storage array of `BITSET8_ARRAY_SIZE` words. Bit `i` is held in
 * `bits[i / UINT8_BITS]` at position `i % UINT8_BITS`. Zero-initialised by
 * `bitset8_init()`.
 *
 * @param size
 * The number of `uint8_t` words currently in use, as computed by
 * `BITSET8_WORDS_SIZE()` from the length requested at construction. A value of 0
 * is the validity sentinel: it means the bitset is empty, was constructed from
 * `len == 0`, or was invalidated by `bitset8_free()`. This field is NOT updated
 * by `_set`, `_clear` or `_isvalid`.
 *
 * @param count
 * A running tally maintained by `bitset8_set()` (incremented on every call) and
 * `bitset8_clear()` (decremented on every call, but never below 0). It is NOT a
 * count of distinct set bits: setting an already-set bit still increments it,
 * and clearing an already-clear bit still decrements it (down to the 0 floor).
 * Being a `uint8_t`, it wraps from 255 back to 0 after 256 increments. It
 * therefore equals the number of distinct set bits only when the caller never
 * repeats a `_set` on a set bit nor a `_clear` on a clear bit, and the tally
 * stays within `0 .. 255`.
 */
struct bitset8_s
{
    uint8_t bits[BITSET8_ARRAY_SIZE] ;
    uint8_t size ;
    uint8_t count ;
} ;
typedef struct bitset8_s bitset8_t ;

/**
 * @brief Static initialiser for a zeroed (empty, invalid) `bitset8_t`.
 *
 * Expands to a brace initialiser that clears the `bits` array and sets both
 * `size` and `count` to 0. The resulting bitset has `size == 0`, i.e. it is in
 * the empty/invalid state until populated by `bitset8_init()`.
 */
#define BITSET8_ZERO { {0}, 0, 0 }

/**
 * @brief Construct a zeroed bitset sized to hold @p len bits.
 *
 * Returns, by value, a `bitset8_t` whose `bits` array is entirely zero and
 * whose `count` is 0. The `size` field is set to `BITSET8_WORDS_SIZE(len)`, the
 * number of `uint8_t` words needed to hold @p len bits. No allocation is
 * performed and `errno` is never modified.
 *
 * @param[in] len
 * The requested number of bits (0..255). `len == 0` yields a word count of 0,
 * producing a bitset with `size == 0`.
 *
 * @return A `bitset8_t` value:
 *      - on success, `bits` all 0, `count == 0`, and
 *        `size == BITSET8_WORDS_SIZE(len)` (a value in `1 .. BITSET8_ARRAY_SIZE`)
 *      - when `len == 0`, the all-zero `BITSET8_ZERO` value, identified by
 *        `size == 0`
 *
 * @note This function does not report failure through `errno`; the only failure
 *       signal is `size == 0` in the returned value. The capacity-rejection
 *       branch (`BITSET8_WORDS_SIZE(len) > BITSET8_ARRAY_SIZE`) present in the
 *       implementation cannot be triggered through this API, because a `uint8_t`
 *       @p len caps the word count at `BITSET8_ARRAY_SIZE`.
 */
extern bitset8_t bitset8_init(uint8_t len) ;

/**
 * @brief Set the bit at @p index to 1 and bump the running tally.
 *
 * Sets bit @p index in `@bitset->bits` (word `index / UINT8_BITS`, position
 * `index % UINT8_BITS`) and unconditionally increments `@bitset->count`.
 *
 * @param[in,out] bitset
 * Pointer to the bitset to modify. Must not be NULL; the function dereferences
 * it without a NULL check.
 *
 * @param[in] index
 * The bit to set.
 *
 * @note No bounds checking is performed: an @p index of `size * UINT8_BITS` or
 *       greater writes outside the in-use words, and an @p index of
 *       `BITSET8_ARRAY_SIZE * UINT8_BITS` or greater writes out of bounds of the
 *       `bits` array (undefined behaviour). The caller must keep @p index in
 *       range.
 * @note `count` is incremented on every call, including when @p index was
 *       already set, and wraps at 256; it is a call tally, not a population
 *       count. See `bitset8_s::count`.
 */
extern void bitset8_set(bitset8_t *bitset, uint8_t index) ;

/**
 * @brief Clear the bit at @p index to 0 and decrement the running tally.
 *
 * Clears bit @p index in `@bitset->bits` (word `index / UINT8_BITS`, position
 * `index % UINT8_BITS`) and decrements `@bitset->count`, but only when `count`
 * is non-zero, so `count` never underflows below 0.
 *
 * @param[in,out] bitset
 * Pointer to the bitset to modify. Must not be NULL; the function dereferences
 * it without a NULL check.
 *
 * @param[in] index
 * The bit to clear.
 *
 * @note No bounds checking is performed: an @p index at or beyond
 *       `BITSET8_ARRAY_SIZE * UINT8_BITS` accesses the `bits` array out of
 *       bounds (undefined behaviour). The caller must keep @p index in range.
 * @note `count` is decremented on every call where `count > 0`, including when
 *       @p index was already clear; it is a call tally, not a population count.
 *       See `bitset8_s::count`.
 */
extern void bitset8_clear(bitset8_t *bitset, uint8_t index) ;

/**
 * @brief Test whether the bit at @p index is set.
 *
 * Returns the result of masking word `index / UINT8_BITS` of `@bitset->bits`
 * with `1U << (index % UINT8_BITS)`.
 *
 * @param[in] bitset
 * Pointer to the bitset to read. Must not be NULL; the function dereferences it
 * without a NULL check.
 *
 * @param[in] index
 * The bit to test.
 *
 * @return The masked word value:
 *      - non-zero (specifically `1 << (index % UINT8_BITS)`, a positive value in
 *        `1 .. 128`) when the bit is set
 *      - 0 when the bit is clear
 *
 * @note Callers should treat the result as a boolean (compare against 0); the
 *       exact non-zero value carries no meaning beyond "set".
 * @note No bounds checking is performed: an @p index at or beyond
 *       `BITSET8_ARRAY_SIZE * UINT8_BITS` reads the `bits` array out of bounds
 *       (undefined behaviour). The caller must keep @p index in range.
 */
extern int bitset8_isvalid(const bitset8_t *bitset, uint8_t index) ;

/**
 * @brief Invalidate a bitset by resetting its size sentinel.
 *
 * If @p bitset is non-NULL and its `size` is greater than 0, sets `size` to 0,
 * marking the bitset empty/invalid. The `bits` array and the `count` field are
 * left untouched, and no memory is released (the bitset owns none).
 *
 * @param[in,out] bitset
 * Pointer to the bitset to invalidate. May be NULL, in which case the function
 * does nothing.
 *
 * @note Despite the name, this performs no deallocation: a `bitset8_t` is a
 *       value type with inline storage. The function only flips the `size`
 *       sentinel to 0; it does NOT clear the `bits` array and does NOT reset
 *       `count`. A bitset that already has `size == 0` is left unchanged.
 */
extern void bitset8_free(bitset8_t *bitset) ;

#endif