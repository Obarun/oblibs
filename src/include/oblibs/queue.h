/*
 * queue.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * Implementation and API for managing a dynamic priority queue using a binary min-heap.
 *
 * General Design:
 *  - Heap-based Priority Queue:
 *    The queue is implemented as a binary min-heap, ensuring that the smallest element is always accessible at the root.
 *    This structure supports efficient insertion, removal, and retrieval operations, making it ideal for scenarios requiring
 *    prioritized task management or data processing.
 *
 *  - Memory Management:
 *    The queue dynamically adjusts its allocated memory based on its size. When full, the queue automatically resizes
 *    to accommodate more elements. Conversely, it shrinks when the size decreases significantly to optimize memory usage.
 *    Functions like 'queue_free' ensure proper cleanup of allocated memory.
 *
 *  - Error Handling:
 *    Functions return meaningful error codes ('EINVAL', 'ENOMEM', etc.) using 'errno' for diagnostics.
 *    This allows developers to handle allocation failures, invalid inputs, and other exceptional cases robustly.
 *
 *  - Customization:
 *    The queue supports custom comparison functions ('cmp' and 'rmcmp') to maintain heap properties and enable tailored
 *    element removal strategies.
 *
 * Usage Scenarios:
 *  This queue is suitable for applications requiring dynamic and efficient priority-based task or data management:
 *  - Scheduling tasks or events based on priority.
 *  - Managing a dynamic list of elements with frequent insertions and deletions.
 *  - Implementing algorithms like Dijkstra's shortest path or A*.
 *
 * Core Structure:
 *  The 'queue_t' structure represents the dynamic priority queue:
 *
 *  typedef struct queue_s {
 *      void **data;         // Array of pointers to elements in the queue.
 *      compare_func_t cmp;  // Comparison function for maintaining the heap property.
 *      compare_func_t rmcmp;// Comparison function for element removal.
 *      size_t size;         // Current number of elements in the queue.
 *      uint32_t capacity;   // Maximum capacity of the queue before resizing.
 *  } queue_t;
 *
 * Macros:
 *  - FOREACH_QUEUE(q, c): Iterates through the elements of the queue.
 *    Usage: `FOREACH_QUEUE(queue, element)` allows traversal of all elements in the queue.
 *  - CLEAN_FREE(q): Safely frees memory and sets the pointer to NULL.
 *    Ensures that double-free errors are avoided.
 *  - QUEUE_PARENT(i): Computes the parent index of a given node.
 *  - QUEUE_LEFT(i): Computes the left child index of a given node.
 *  - QUEUE_RIGHT(i): Computes the right child index of a given node.
 */


#ifndef OB_QUEUE_H
#define OB_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#define NPLUS 8

#define FOREACH_QUEUE(q, c) \
    for (size_t _i = 0; (q) && _i < (q)->size && ((c) = (q)->data[_i], 1); _i++)

#ifndef CLEAN_FREE
#define CLEAN_FREE(q) { free(q); q = NULL; }
#endif

typedef int (*compare_func_t)(const void *, const void *);

/**
 * @struct queue_t
 * @brief Represents a queue implemented as a binary min-heap.
 *
 * The queue is a dynamic data structure that organizes elements in a binary
 * min-heap. This ensures that the smallest element is always accessible
 * at the root. It supports insertion, deletion, resizing, and heap-based
 * sorting operations.
 *
 * @param data
 * A pointer to an array of void pointers storing the elements in the queue.
 *
 * @param cmp
 * A function pointer to a comparison function used to maintain the heap property.
 * The function should return:
 * - A negative value if the first element is less than the second.
 * - Zero if the elements are equal.
 * - A positive value if the first element is greater than the second.
 *
 * @param rmcmp
 * A function pointer to a comparison function used to retrieve
 * a same element for removal operation.
 * The function should return:
 * - Zero if the elements are equal.
 *
 * @param size
 * The current number of elements in the queue.
 *
 * @param capacity
 * The maximum number of elements the queue can hold before requiring a resize.
 */
typedef struct queue_s queue_t;
struct queue_s {
    void **data ;
    compare_func_t cmp ;
    compare_func_t rmcmp ;
    size_t size ;
    uint32_t capacity ;
} ;

#define QUEUE_PARENT(i) (((i) - 1) / 2)
#define QUEUE_LEFT(i) ((2 * (i)) + 1)
#define QUEUE_RIGHT(i) ((2 * (i)) + 2)

/**
 * @brief Initializes a queue with the given capacity and comparison functions.
 *
 * Allocates memory for a new queue structure and its internal array. If the capacity is zero,
 * it defaults to the macro-defined value `NPLUS`. The queue requires two comparison functions:
 * - `cmp`: Used for ordering elements in the queue to maintain the heap property.
 * - `rmcmp`: Used for custom removal logic during element deletion.
 *
 * @param capacity
 * Initial capacity of the queue. Defaults to `NPLUS` if set to zero.
 *
 * @param cmp
 * Pointer to a comparison function for ordering elements in the queue.
 * The function should return:
 * - A negative value if the first element is less than the second.
 * - Zero if the elements are equal.
 * - A positive value if the first element is greater than the second.
 *
 * @param rmcmp
 * Pointer to a comparison function used for custom removal operations.
 *
 * @return
 * A pointer to the initialized queue on success.
 * Returns `NULL` on failure and sets `errno` as follows:
 * - `EINVAL` if either `cmp` or `rmcmp` is `NULL`.
 * - `ENOMEM` if memory allocation fails.
 *
 * @note
 * - If allocation of the queue or its internal array fails, the function frees all allocated memory and returns `NULL`.
 * - Both `cmp` and `rmcmp` are required; passing `NULL` for either results in failure.
 */
extern queue_t *queue_init(uint32_t capacity, compare_func_t cmp, compare_func_t rmcmp) ;

/**
 * @brief Swaps the values of two pointers in the queue.
 *
 * Used to exchange two elements in the queue, typically while reordering
 * elements to maintain heap properties.
 *
 * @param a
 * Pointer to the first element.
 *
 * @param b
 * Pointer to the second element.
 */
extern void queue_swap(void **a, void **b) ;

/**
 * @brief Moves an element up in the heap to restore the heap property.
 *
 * This function is used to restore the heap property of the queue after an element
 * has been inserted or modified. Starting from the specified index, it compares the
 * element with its parent and swaps them if the heap property is violated. The process
 * continues until the heap property is satisfied or the element reaches the root.
 *
 * @param q
 * Pointer to the queue where the operation is performed.
 *
 * @param index
 * Index of the element to move up in the heap.
 *
 * @note
 * - The heap property ensures that the parent node is always smaller (in a min-heap) or
 *   larger (in a max-heap) than its child nodes, depending on the comparison function.
 * - This function assumes that the index is valid and within the bounds of the queue's size.
 */
extern void queue_up(queue_t *q, size_t index) ;

/**
 * @brief Moves an element down in the heap to restore the heap property.
 *
 * This function is used to restore the heap property of the queue after an element
 * has been removed or modified. Starting from the specified index, it compares the
 * element with its children and swaps it with the smallest child if the heap property
 * is violated. The process continues until the heap property is satisfied or the element
 * reaches a leaf node.
 *
 * @param q
 * Pointer to the queue where the operation is performed.
 *
 * @param index
 * Index of the element to move down in the heap.
 *
 * @note
 * - The heap property ensures that the parent node is always smaller (in a min-heap) or
 *   larger (in a max-heap) than its child nodes, depending on the comparison function.
 * - This function assumes that the index is valid and within the bounds of the queue's size.
 */
extern void queue_down(queue_t *q, size_t index) ;

/**
 * @brief Resizes the queue to accommodate more elements.
 *
 * This function increases the allocated memory for the queue's internal array if the
 * number of elements equals its current capacity. The new capacity is determined by
 * adding a predefined increment (`NPLUS`) to the current capacity.
 *
 * @param q
 * Pointer to the queue to resize.
 *
 * @return 1
 * On success, indicating that the queue has been resized.
 *
 * @return 0
 * On failure, with `errno` set to:
 * - `EINVAL` if the queue or its data array is invalid.
 * - `ENOMEM` if memory allocation fails during resizing.
 *
 * @note
 * - The function does nothing if the queue is not at full capacity.
 * - The `q->data` pointer is updated to the newly allocated memory block on success.
 */
extern int queue_resize(queue_t *q) ;

/**
 * @brief Inserts a new element into the queue.
 *
 * This function adds a new element to the queue while maintaining the heap property.
 * If the queue is full, it automatically resizes the internal array. The element is
 * copied into a newly allocated memory block before insertion to ensure safe ownership.
 *
 * @param q
 * Pointer to the queue where the element will be inserted.
 *
 * @param data
 * Pointer to the element to insert.
 *
 * @param dsize
 * Size of the element in bytes.
 *
 * @return 1
 * On success, indicating that the element was inserted and the heap property is maintained.
 *
 * @return 0
 * On failure, with `errno` set to:
 * - `EINVAL` if the queue or data pointer is invalid.
 * - `ENOMEM` if memory allocation fails during resizing or element copying.
 *
 * @note
 * - The function ensures that the heap property is restored after insertion using `queue_up`.
 * - The `data` pointer is copied into a newly allocated memory block, and the caller retains
 *   ownership of the original `data`.
 */
extern int queue_insert(queue_t *q, void *data, size_t dsize) ;

/**
 * @brief Removes a specific element from the queue.
 *
 * This function searches for the specified element in the queue using the removal
 * comparison function (`rmcmp`). If the element is found, it is removed, and the
 * heap property is restored. The queue may also shrink its allocated memory if
 * the size significantly decreases.
 *
 * @param q
 * Pointer to the queue from which the element will be removed.
 *
 * @param data
 * Pointer to the element to remove.
 *
 * @return 1
 * If the element is successfully removed.
 *
 * @return 0
 * If the element is not found, or if the queue is invalid (`errno` is set to `EINVAL`).
 *
 * @note
 * - The function uses `rmcmp` to identify the element to remove.
 * - The heap property is maintained after removal using `queue_down`.
 * - Memory for the removed element is freed, and the queue may shrink if appropriate.
 */
extern int queue_remove(queue_t *q, void *data) ;

/**
 * @brief Retrieves the smallest element in the queue.
 *
 * This function returns the smallest element in the queue, located at the root
 * of the heap. It does not modify the queue.
 *
 * @param q
 * Pointer to the queue.
 *
 * @return
 * Pointer to the smallest element in the queue, or `NULL` if the queue is invalid
 * or empty (`errno` is set to `EINVAL`).
 *
 * @note
 * - The function assumes the heap property is maintained in the queue.
 * - The queue remains unchanged after this operation.
 */
extern void *queue_get_min(queue_t *q) ;

/**
 * @brief Sorts the elements in the queue in ascending order.
 *
 * This function sorts the elements in the queue using heap sort. It temporarily
 * modifies the heap structure to arrange elements in ascending order, then restores
 * the original structure.
 *
 * @param q
 * Pointer to the queue to sort.
 *
 * @return 1
 * If the queue is successfully sorted.
 *
 * @return 0
 * If the queue is invalid or contains one or no elements (`errno` is set to `EINVAL`).
 *
 * @note
 * - The sorting process temporarily breaks the heap property.
 * - The original heap structure and size are restored after sorting.
 * - The function also reverses the array to ensure ascending order in a min-heap.
 */
extern int queue_sort(queue_t *q) ;

/**
 * @brief Reverses the order of elements in the queue.
 *
 * This function reverses the order of the elements in the queue's internal array.
 * It swaps elements from both ends of the array, moving towards the center, to
 * reverse their order.
 *
 * @param q
 * Pointer to the queue whose elements will be reversed.
 *
 * @return 1
 * If the queue is successfully reversed or has one or no elements (no reversal needed).
 *
 * @return 0
 * If the queue is invalid (`errno` is set to `EINVAL`).
 *
 * @note
 * - The function does nothing if the queue is empty or contains a single element.
 * - The heap property is not maintained after reversal.
 */
extern int queue_reverse(queue_t *q) ;

/**
 * @brief Shrinks the allocated capacity of the queue if its size significantly decreases.
 *
 * This function reduces the allocated memory for the queue's internal array
 * when the number of elements is less than half its current capacity. The new
 * capacity is set to the current size plus an additional buffer (`NPLUS`).
 *
 * @param q
 * Pointer to the queue to shrink.
 *
 * @return 1
 * If the queue's capacity was successfully adjusted or no shrinking was needed.
 *
 * @return 0
 * If the queue is invalid or memory reallocation fails (`errno` is set to `EINVAL` or `ENOMEM`).
 *
 * @note
 * - If shrinking fails, the queue's capacity and data remain unchanged.
 * - The function ensures that shrinking only occurs when the size is less than half the capacity.
 */
extern int queue_shrink(queue_t *q) ;

/**
 * @brief Frees all memory associated with the queue.
 *
 * This function releases the memory allocated for the queue, including the
 * internal array and all elements stored in it. After calling this function,
 * the queue pointer should no longer be used.
 *
 * @param q
 * Pointer to the queue to free.
 *
 * @note
 * - If the queue or its internal array is `NULL`, the function does nothing.
 * - Each element in the internal array is freed before freeing the array itself.
 */
extern void queue_free(queue_t *q) ;

#endif