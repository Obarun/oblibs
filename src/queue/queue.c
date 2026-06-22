/*
 * queue.c
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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <oblibs/queue.h>

queue_t *queue_init(uint32_t capacity, compare_func_t cmp, compare_func_t rmcmp)
{
    if (!cmp || !rmcmp)
        return (errno = EINVAL, NULL) ;

    uint32_t cap = capacity ;
    if (!cap)
        cap = NPLUS ;

    queue_t *q = malloc(sizeof(queue_t)) ;
    if (!q)
        return NULL ;

    q->data = malloc(sizeof(void *) * cap) ;
    if (!q->data)
        CLEAN_FREE(q) ;

    q->cmp = cmp ;
    q->rmcmp = rmcmp ;
    q->size = 0 ;
    q->capacity = cap ;

    return q ;
}

void queue_swap(void **a, void **b)
{
    void *temp = *a ;
    *a = *b ;
    *b = temp ;
}

void queue_up(queue_t *q, size_t index)
{
    while (index > 0) {
        size_t parent = QUEUE_PARENT(index) ;
        if (q->cmp(q->data[index], q->data[parent]) >= 0)
            break;

        queue_swap(&q->data[index], &q->data[parent]) ;
        index = parent ;
    }
}

void queue_down(queue_t *q, size_t index)
{
    size_t smallest = index;

    while (1) {
        size_t left = QUEUE_LEFT(index) ;
        size_t right = QUEUE_RIGHT(index) ;

        if (left < q->size && q->cmp(q->data[left], q->data[smallest]) < 0)
            smallest = left ;

        if (right < q->size && q->cmp(q->data[right], q->data[smallest]) < 0)
            smallest = right ;

        if (smallest == index)
            break ;

        queue_swap(&q->data[index], &q->data[smallest]) ;
        index = smallest ;
    }
}

int queue_resize(queue_t *q)
{
    if (!q || !q->data)
        return (errno = EINVAL, 0) ;

    if (q->size == q->capacity) {

        size_t new_capacity = q->capacity + NPLUS ;
        void **new = realloc(q->data, new_capacity * sizeof(void *)) ;
        if (!new)
            return (errno = ENOMEM, 0) ;

        q->data = new ;
        q->capacity = new_capacity ;
    }

    return 1 ;
}

int queue_insert(queue_t *q, void *data, size_t dsize)
{
    if (!q || !data)
        return (errno = EINVAL, 0) ;

    if (q->size >= q->capacity)
        if (!queue_resize(q))
            return 0 ;

    void *d = malloc(dsize) ;
    if (!d)
        return (errno = ENOMEM, 0) ;

    memcpy(d, data, dsize);
    q->data[q->size] = d ;
    queue_up(q, q->size) ;
    q->size++ ;

    return 1 ;
}

int queue_remove(queue_t *q, void *data)
{
    if (!q || q->size == 0)
        return (errno = EINVAL, 0) ;

    size_t pos = 0 ;
    for (; pos < q->size; pos++) {

        if (q->rmcmp((void *)q->data[pos], (void *)data) == 0) {

            CLEAN_FREE(q->data[pos]) ;

            if (pos != q->size - 1)
                queue_swap(&q->data[pos], &q->data[q->size - 1]) ;

            q->size-- ;

            if (pos < q->size)
                queue_down(q, pos) ;

            if (!queue_shrink(q))
                return (errno = ENOMEM, 0) ;

            return 1 ;
        }
    }

    return 0 ;
}

void *queue_get_min(queue_t *q)
{
    if (!q || q->size == 0)
        return (errno = EINVAL, NULL) ;

    return q->data[0] ;
}

int queue_sort(queue_t *q)
{
    if (!q || q->size < 1)
        return (errno = EINVAL, 0) ;

    if (q->size == 1)
        return 1 ;

    size_t original_size = q->size, pos = q->size - 1, left = 0 ;
    for (; pos > 0; pos--) {
        queue_swap(&q->data[0], &q->data[pos]) ;
        q->size-- ;
        queue_down(q, 0) ;
    }

    q->size = original_size ;

    size_t right = q->size - 1;
    while (left < right) {
        queue_swap(&q->data[left], &q->data[right]);
        left++;
        right--;
    }

    return 1 ;
}

int queue_reverse(queue_t *q)
{
    if (!q)
        return (errno = EINVAL, 0) ;

    if (q->size <= 1)
        return 1 ; // Nothing to reverse for empty  or single-element queue

    size_t left = 0 ;
    size_t right = q->size - 1 ;

    while (left < right) {
        queue_swap(&q->data[left], &q->data[right]);
        left++;
        right--;
    }

    return 1 ;
}

int queue_shrink(queue_t *q)
{
    if (!q || !q->data)
       return (errno = EINVAL, 0) ;

    if (q->size >= q->capacity / 2)
        return 1 ;

    int new_capacity = q->size + NPLUS ;
    void **new_data = realloc(q->data, sizeof(void *) * new_capacity);

    if (!new_data)
        return 0 ;

    q->data = new_data ;
    q->capacity = new_capacity ;

    return 1 ;
}

void queue_free(queue_t *q)
{
    if (!q)
        return ;

    if (q->data != NULL) {

        for (size_t i = 0 ; i < q->size ; i++) {
            CLEAN_FREE(q->data[i]) ;
        }

        CLEAN_FREE(q->data) ;
    }

    CLEAN_FREE(q) ;
}

int queue_insert_ptr(queue_t *q, void *data)
{
    if (!q || !data)
        return (errno = EINVAL, 0);

    if (q->size >= q->capacity)
        if (!queue_resize(q))
            return 0;

    q->data[q->size] = data;
    queue_up(q, q->size);
    q->size++;

    return 1;
}

int queue_remove_ptr(queue_t *q, void *data)
{
    if (!q || q->size == 0)
        return (errno = EINVAL, 0);

    size_t pos = 0;
    for (; pos < q->size; pos++) {
        if (q->data[pos] == data) {

            if (pos != q->size - 1)
                queue_swap(&q->data[pos], &q->data[q->size - 1]);

            q->size--;

            if (pos < q->size)
                queue_down(q, pos);

            if (!queue_shrink(q))
                return (errno = ENOMEM, 0);

            return 1;
        }
    }

    return 0;
}

void queue_free_ptr(queue_t *q)
{
    if (!q)
        return;

    if (q->data != NULL) {
        CLEAN_FREE(q->data);
    }

    CLEAN_FREE(q);
}