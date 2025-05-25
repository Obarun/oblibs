/*
 * stack.h
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
 *
 * Implementation and API for managing a flexible stack structure.
 *
 * General Design:
 *  - Memory Management:
 *    The stack can either use pre-allocated memory or dynamically allocate memory as needed. When
 *    dynamically allocated, it is essential to free the memory using'stack_free()' to avoid leaks. To ensure proper cleanup,
 *    always call 'stack_free()' at the end of use, even though the memory allocation is handled automatically.
 *    Once allocated, the memory is never reallocated. Dynamic allocation occurs only if the length register
 *    exceeds MAXENV + 3.
 *
 *  - Data Storage:
 *    Strings are stored in a null-terminated format within a continuous memory block. Each string
 *    is separated by a null character, and operations like adding or removing elements adjust the
 *    layout accordingly.
 *
 *  - Error Handling:
 *    Most functions return '1' on success and '0' on failure, with 'errno' set to indicate the
 *    specific error. Common errors include invalid arguments (`EINVAL`) and memory overflows
 *    (`EOVERFLOW`).
 *
 *  - Usage Scenarios:
 *    These functions are suitable for applications requiring efficient management of lists of strings,
 *    such as building command-line arguments, managing configurations, or parsing input data.
 *
 * Stack Structure:
 *  The 'stack_s' structure is defined as follows:
 *
 *  typedef struct stack_s {
 *      char *s;            // Pointer to the memory block storing the stack elements.
 *      size_t len;         // Current length of the stack (number of bytes used).
 *      size_t maxlen;      // Maximum length of the stack (total capacity in bytes).
 *      size_t count;       // Number of elements currently stored in the stack.
 *      uint8_t allocated;  // Flag indicating if the memory was dynamically allocated.
 *  } stack, *stack_ref;
 *
 * Macros:
 *  The following macros are defined for convenience:
 *  - STACK_ZERO: Initializes a stack structure with zero values.
 *    #define STACK_ZERO { .s = 0, .len = 0, .maxlen = 0, .count = 0, .allocated = 0 }
 *
 *  - _cleanup_stk_: Attribute for automatic cleanup of stack resources.
 *    #define _cleanup_stk_ __attribute__((cleanup(stack_free)))
 *
 *  - _alloc_stk_: Macro to allocate and initialize a stack with a specified length.
 *    #define _alloc_stk_(stk, len) \
 *        _cleanup_stk_ stack stk = STACK_ZERO; \
 *        char stk##__store__[((len) + 3) < MAXENV ? ((len) + 3) : 0]; \
 *        init_stack(&stk, (len) < MAXENV ? stk##__store__ : 0, (len))
 */

#ifndef OB_STACK_H
#define OB_STACK_H

#include <string.h>
#include <stdint.h>
#include <unistd.h>

#ifndef MAXENV
#define MAXENV 8191
#endif

#define _cleanup_stk_ __attribute__((cleanup(stack_free)))

#define _alloc_stk_(stk, len) _cleanup_stk_ stack stk = STACK_ZERO ; char stk##__store__[((len) + 3) < MAXENV ? ((len) + 3) : 1]  ; init_stack(&stk, stk##__store__, (len))

#define STACK_ZERO { .s = 0, .len = 0, .maxlen = 0, .count = 0, .allocated = 0 }

#define FOREACH_STK(stk,pos) \
    stack_ref __stk__ref__ = stk ; \
    for (; (size_t)pos < __stk__ref__->len ; pos += (size_t)strlen(__stk__ref__->s + (size_t)pos) + 1)

typedef struct stack_s stack, *stack_ref ;
struct stack_s
{
    char *s ;
    size_t len ;
    size_t maxlen ;
    size_t count ;
    uint8_t allocated ;
} ;

/** See also, lexer.h for stack_string_XXX function */

/**
 * @brief Initializes a stack with the given storage and size.
 *
 * This function initializes a stack structure, either allocating memory dynamically or
 * using the provided storage. It prepares the stack for operations.
 *
 * @param stk
 * Pointer to the stack structure to initialize.
 *
 * @param store
 * Pointer to the memory to use for the stack. If NULL, memory will be dynamically allocated.
 *
 * @param len
 * Length of the memory for the stack.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 * - ERANGE: Memory allocation failed.
 *
 * @note
 * Ensure the stack is properly freed using stack_free() if dynamically allocated.
 */
extern uint8_t init_stack(stack *stk,char *store, size_t len) ;

/**
 * @brief Frees resources allocated for the stack.
 *
 * If the stack was dynamically allocated, this function releases the memory.
 *
 * @param s
 * Pointer to the stack structure to free.
 */
extern void stack_free(stack *s) ;

/**
 * @brief Resets the stack to an empty state.
 *
 * This function clears the contents of the stack without deallocating memory.
 *
 * @param stk
 * Pointer to the stack structure to reset.
 *
 * @note
 * This operation preserves the allocated memory of the stack but resets its length and count to zero.
 */
extern void stack_reset(stack *stk) ;

/**
 * @brief Adds a string to the stack.
 *
 * This function appends a string to the stack, updating its length and count.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param str
 * String to add to the stack.
 *
 * @param len
 * Length of the string.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 * - EOVERFLOW: Stack memory exceeded.
 *
 * @note
 * This function does not append a null terminator; use stack_close() to ensure the stack is null-terminated.
 */
extern uint8_t stack_add(stack *stk, char const *string, size_t len) ;


/**
 * @brief Adds a null-terminated string to the stack.
 *
 * This function appends a null-terminated string to the stack.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param str
 * Null-terminated string to add.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 */
extern uint8_t stack_add_g(stack *stk, char const *str) ;

/**
 * @brief Closes the stack by appending a null character.
 *
 * This function finalizes the stack for use, ensuring it is null-terminated.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 */
extern uint8_t stack_close(stack *stk) ;

/**
 * @brief Copies a string into the stack.
 *
 * This function replaces the stack's contents with a specified string.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param string
 * String to copy into the stack.
 *
 * @param len
 * Length of the string.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 * - EOVERFLOW: Stack memory exceeded.
 */
extern uint8_t stack_copy(stack *stk, char const *string, size_t len) ;

#define stack_copy_g(stack, s) stack_copy(stack, (s), strlen(s))
#define stack_copy_stack(dst, src) stack_copy(dst, (src)->s, (src)->len)

/**
 * @brief Counts the number of elements in the stack.
 *
 * This function iterates through the stack to count the stored elements.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @return size_t
 * Returns the number of elements in the stack.
 */
extern size_t stack_count_element(stack *stk) ;

/**
 * @brief Retrieves the position of an element in the stack.
 *
 * This function searches for a string in the stack and returns its position.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param string
 * String to search for.
 *
 * @return ssize_t
 * Returns the position of the string, or -1 if not found.
 */
extern ssize_t stack_retrieve_element(stack *stk, char const *string) ;

/**
 * @brief Removes an element from the stack by index.
 *
 * This function removes an element at the specified index from the stack.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param index
 * Index of the element to remove.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 */
extern uint8_t stack_remove_element(stack *stk, size_t index) ;


/**
 * @brief Removes an element from the stack by value.
 *
 * This function searches for a string in the stack and removes it.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param element
 * String to remove.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 */
extern uint8_t stack_remove_element_g(stack *stk, char const *element) ;

/**
 * @brief Rebuilds the stack with a delimiter.
 *
 * This function reconstructs the stack using a specified delimiter.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param delim
 * Delimiter to use for rebuilding the stack.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 * - EOVERFLOW: Stack memory exceeded.
 */
extern uint8_t stack_string_rebuild_with_delim(stack *stk, unsigned int delim) ;

/**
 * @brief Inserts a string into the stack at a specified offset.
 *
 * This function inserts a string at a specific position in the stack.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param offset
 * Offset where the string will be inserted.
 *
 * @param s
 * String to insert.
 *
 * @return uint8_t
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - EINVAL: Invalid arguments.
 */
extern uint8_t stack_insert(stack *stk, size_t offset, char const *s) ;

/**
 * @brief Reads the contents of a file into the stack.
 *
 * This function reads data from a specified file and stores it in the stack.
 *
 * @param stk
 * Pointer to the stack structure.
 *
 * @param path
 * Path to the file to read.
 *
 * @return int
 * Returns 1 on success, or 0 on failure with errno set appropriately.
 * Possible errors:
 * - ENOBUFS: Insufficient buffer space in the stack.
 * - EIO: Input/output error while reading the file.
 */
extern int stack_read_file(stack *stk, const char *path) ;

#endif
