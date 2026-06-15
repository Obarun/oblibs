#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <oblibs/queue.h>

int compare_ints(const void *a, const void *b) {
    const uint32_t ua = (*(uint32_t *)a);
    const uint32_t ub = (*(uint32_t *)b);

    if (ua < ub)
        return -1 ;
    else if (ua > ub)
        return 1 ;

    return 0 ;
}

void test_queue_display(queue_t *q) {
    if (!q) {
        printf("Queue is NULL.\n");
        return;
    }

    if (q->size == 0) {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue contents (size: %ld, capacity: %d):\n", q->size, q->capacity);
    for (size_t i = 0; i < q->size; i++) {
        uint32_t *u = (uint32_t *)q->data[i];
        printf("Index %ld: Priority = %d\n", i, *u);
    }
}


void test_queue_init() {
    printf("Testing queue_init...\n");
    queue_t *q = queue_init(4, compare_ints, compare_ints);
    assert(q != NULL);
    assert(q->data != NULL) ;
    assert(q->size == 0);
    assert(q->capacity == 4);
    queue_free(q);
}

void test_queue_init_empty_cap() {
    printf("Testing queue_init_empty_cap...\n");
    queue_t *q = queue_init(0, compare_ints, compare_ints);
    assert(q != NULL);
    assert(q->data != NULL) ;
    assert(q->size == 0);
    assert(q->capacity == NPLUS);
    queue_free(q);
}

void test_queue_insert() {
    printf("Testing queue_insert...\n");
    queue_t *q = queue_init(4, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert(q, &a, sizeof(uint32_t)));
    assert(queue_insert(q, &b, sizeof(uint32_t)));
    assert(queue_insert(q, &c, sizeof(uint32_t)));

    uint32_t *min = (uint32_t *)queue_get_min(q);
    assert(min != NULL && *min == 5);

    queue_free(q);
}

void test_queue_remove() {
    printf("Testing queue_remove...\n");
    queue_t *q = queue_init(4, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert(q, &a, sizeof(uint32_t)));
    assert(queue_insert(q, &b, sizeof(uint32_t)));
    assert(queue_insert(q, &c, sizeof(uint32_t)));

    assert(queue_remove(q, &b));
    uint32_t *min = (uint32_t *)queue_get_min(q);
    assert(min != NULL && *min == 10);

    assert(queue_remove(q, &b) == 0); // Element already removed
    assert(q->size == 2);

    queue_free(q);
}

void test_queue_remove_ptr() {
    printf("Testing queue_remove_ptr...\n");
    queue_t *q = queue_init(4, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert_ptr(q, &a));
    assert(queue_insert_ptr(q, &b));
    assert(queue_insert_ptr(q, &c));

    assert(queue_remove_ptr(q, &b));
    uint32_t *min = (uint32_t *)queue_get_min(q);
    assert(min != NULL && *min == 10);

    assert(queue_remove_ptr(q, &b) == 0); // Element already removed
    assert(q->size == 2);

    queue_free_ptr(q);
}

void test_queue_sort() {
    printf("Testing queue_sort...\n");
    queue_t *q = queue_init(4, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert(q, &a, sizeof(uint32_t)));
    assert(queue_insert(q, &b, sizeof(uint32_t)));
    assert(queue_insert(q, &c, sizeof(uint32_t)));

    assert(queue_sort(q));
    assert(q->size == 3);

    uint32_t *sorted = (uint32_t *)q->data[0];
    assert(*sorted == 5);

    assert(*(uint32_t *)q->data[0] == b); // smaller element at the top
    assert(*(uint32_t *)q->data[1] == a);
    assert(*(uint32_t *)q->data[2] == c);

    queue_free(q);
}

void test_queue_mode() {
    printf("Testing queue_mode...\n");
    queue_t *q = queue_init(10, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert(q, &a, sizeof(uint32_t)));
    assert(queue_insert(q, &b, sizeof(uint32_t)));
    assert(queue_insert(q, &c, sizeof(uint32_t)));

    // Verify heap's min mode behavior
    assert(q->size == 3);
    uint32_t *min = queue_get_min(q);
    assert(min != NULL);
    assert(*min == 5);

    queue_remove(q, &b);
    min = queue_get_min(q);
    assert(min != NULL);
    assert(*min == 10);

    queue_free(q);
}

void test_queue_reverse() {
    printf("Testing queue_reverse...\n");
    queue_t *q = queue_init(4, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert(q, &a, sizeof(uint32_t)));
    assert(queue_insert(q, &b, sizeof(uint32_t)));
    assert(queue_insert(q, &c, sizeof(uint32_t)));

    // Assert sorted order (ascending)
    assert(*(uint32_t *)q->data[0] == b);
    assert(*(uint32_t *)q->data[1] == a);
    assert(*(uint32_t *)q->data[2] == c);

    assert(queue_sort(q));
    assert(queue_reverse(q));

    assert(*(uint32_t*)q->data[0] == c);
    assert(*(uint32_t*)q->data[1] == a);
    assert(*(uint32_t*)q->data[2] == b);

    queue_free(q);
}

void test_queue_shrink() {
    printf("Testing queue_shrink...\n");
    queue_t *q = queue_init(8, compare_ints, compare_ints);
    assert(q != NULL);

    uint32_t a = 10, b = 5, c = 20;
    assert(queue_insert(q, &a, sizeof(uint32_t)));
    assert(queue_insert(q, &b, sizeof(uint32_t)));
    assert(queue_insert(q, &c, sizeof(uint32_t)));

    assert(q->capacity == 8);
    assert(q->size == 3);

    while (q->size > 1) {
        assert(queue_remove(q, q->data[q->size - 1]));
    }

    assert(queue_shrink(q));
    assert(q->capacity == q->size + NPLUS);

    queue_free(q);
}

int main() {
    test_queue_init();
    test_queue_init_empty_cap();
    test_queue_insert();
    test_queue_remove();
    test_queue_remove_ptr();
    test_queue_sort();
    test_queue_mode();
    test_queue_reverse();
    test_queue_shrink();
    printf("All tests passed successfully.\n") ;
    return 0;
}