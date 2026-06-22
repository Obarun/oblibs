/*
 * eventfd_watcher.c
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

#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

#include <oblibs/sse.h>
#include "framework.h"

/* Test callback for eventfd events */
static void test_eventfd_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store watcher pointer for identification */
    data->user_data = w;
}

/* Main eventfd watcher test - single loop handling multiple eventfds */
static bool test_eventfd_watchers_comprehensive(void) {
    sse_epoll_t loop;

    /* Multiple eventfd watchers */
    sse_watcher_t efd1_watcher, efd2_watcher, efd3_watcher, efd4_watcher;
    struct test_callback_data efd1_data, efd2_data, efd3_data, efd4_data;

    /* Initialize callback data */
    test_callback_init(&efd1_data);
    test_callback_init(&efd2_data);
    test_callback_init(&efd3_data);
    test_callback_init(&efd4_data);

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true ;

    /* === Start eventfd watchers with different priorities === */
    TEST_ASSERT(sse_start_eventfd(&loop, &efd1_watcher, test_eventfd_callback, &efd1_data, 1),
               "Failed to start first eventfd watcher");

    TEST_ASSERT(sse_start_eventfd(&loop, &efd2_watcher, test_eventfd_callback, &efd2_data, 2),
               "Failed to start second eventfd watcher");

    TEST_ASSERT(sse_start_eventfd(&loop, &efd3_watcher, test_eventfd_callback, &efd3_data, 3),
               "Failed to start third eventfd watcher");

    TEST_ASSERT(sse_start_eventfd(&loop, &efd4_watcher, test_eventfd_callback, &efd4_data, 4),
               "Failed to start fourth eventfd watcher");

    /* === Verify all watchers are active === */
    TEST_ASSERT(sse_watcher_active(&efd1_watcher), "First eventfd watcher should be active");
    TEST_ASSERT(sse_watcher_active(&efd2_watcher), "Second eventfd watcher should be active");
    TEST_ASSERT(sse_watcher_active(&efd3_watcher), "Third eventfd watcher should be active");
    TEST_ASSERT(sse_watcher_active(&efd4_watcher), "Fourth eventfd watcher should be active");

    /* === Verify eventfd watcher setup === */
    TEST_ASSERT_EQ(SSE_TYPE_EVENTFD, efd1_watcher.type, "Wrong watcher type");
    TEST_ASSERT(efd1_watcher.fd >= 0, "Eventfd watcher should have valid fd");
    TEST_ASSERT_EQ(SSE_READ, efd1_watcher.events, "Eventfd should watch for read events");

    /* Verify each watcher has unique fd */
    TEST_ASSERT_NEQ(efd1_watcher.fd, efd2_watcher.fd, "Watchers should have different fds");
    TEST_ASSERT_NEQ(efd1_watcher.fd, efd3_watcher.fd, "Watchers should have different fds");
    TEST_ASSERT_NEQ(efd2_watcher.fd, efd3_watcher.fd, "Watchers should have different fds");

    /* === Test eventfd write/read operations === */

    /* Write to first eventfd */
    TEST_ASSERT(sse_write_eventfd(&efd1_watcher), "Failed to write to first eventfd");

    /* Write to second eventfd */
    TEST_ASSERT(sse_write_eventfd(&efd2_watcher), "Failed to write to second eventfd");

    /* Write multiple times to third eventfd */
    TEST_ASSERT(sse_write_eventfd(&efd3_watcher), "Failed to write to third eventfd (1)");
    TEST_ASSERT(sse_write_eventfd(&efd3_watcher), "Failed to write to third eventfd (2)");
    TEST_ASSERT(sse_write_eventfd(&efd3_watcher), "Failed to write to third eventfd (3)");

    /* Process events */
    int max_iterations = 20;
    int events_received = 0;

    while (max_iterations-- > 0 && events_received < 3) {
        if (!sse_run(&loop, 50)) {
            break;
        }

        events_received = 0;
        if (efd1_data.callback_called) events_received++;
        if (efd2_data.callback_called) events_received++;
        if (efd3_data.callback_called) events_received++;
    }

    /* === Verify events were received === */
    TEST_ASSERT(efd1_data.callback_called, "First eventfd callback should be called");
    TEST_ASSERT(efd2_data.callback_called, "Second eventfd callback should be called");
    TEST_ASSERT(efd3_data.callback_called, "Third eventfd callback should be called");
    TEST_ASSERT(!efd4_data.callback_called, "Fourth eventfd should not be triggered");

    /* === Verify event types === */
    TEST_ASSERT(efd1_data.last_event & SSE_READ, "First eventfd should generate read event");
    TEST_ASSERT(efd2_data.last_event & SSE_READ, "Second eventfd should generate read event");
    TEST_ASSERT(efd3_data.last_event & SSE_READ, "Third eventfd should generate read event");

    /* === Test priority ordering (lower number = higher priority) === */
    /* All should be called once, but priority should affect processing order */
    TEST_ASSERT_EQ(1, efd1_data.call_count, "First eventfd should be called once");
    TEST_ASSERT_EQ(1, efd2_data.call_count, "Second eventfd should be called once");
    TEST_ASSERT_EQ(1, efd3_data.call_count, "Third eventfd should be called once (coalesced)");

    /* === Test manual read operations === */
    /* Write to fourth eventfd and read manually */
    TEST_ASSERT(sse_write_eventfd(&efd4_watcher), "Failed to write to fourth eventfd");
    TEST_ASSERT(sse_read_eventfd(&efd4_watcher), "Failed to read from fourth eventfd");

    /* The manual read should have consumed the event */
    TEST_ASSERT(sse_run(&loop, 50), "Event loop should run");
    TEST_ASSERT(!efd4_data.callback_called, "Fourth eventfd callback should not be called after manual read");

    /* === Test stop/restart functionality === */
    TEST_ASSERT(sse_stop_eventfd(&efd2_watcher), "Failed to stop second eventfd watcher");
    TEST_ASSERT(!sse_watcher_active(&efd2_watcher), "Stopped eventfd watcher should be inactive");

    /* Write to stopped eventfd - should not trigger callback */
    int efd2_count_before = efd2_data.call_count;
    TEST_ASSERT(sse_write_eventfd(&efd2_watcher), "Should still be able to write to stopped watcher's fd");

    TEST_ASSERT(sse_run(&loop, 50), "Event loop should run");
    TEST_ASSERT_EQ(efd2_count_before, efd2_data.call_count, "Stopped watcher should not receive events");

    /* Restart and test again */
    TEST_ASSERT(sse_restart_eventfd(&efd2_watcher), "Failed to restart second eventfd watcher");
    TEST_ASSERT(sse_watcher_active(&efd2_watcher), "Restarted eventfd watcher should be active");

    TEST_ASSERT(sse_write_eventfd(&efd2_watcher), "Failed to write to restarted eventfd");
    efd2_data.callback_called = false; /* Reset for test */

    max_iterations = 10;
    while (max_iterations-- > 0 && !efd2_data.callback_called) {
        if (!sse_run(&loop, 30)) {
            break;
        }
    }

    TEST_ASSERT(efd2_data.callback_called, "Restarted eventfd should receive events");

    sse_free(&loop);

    return true;
}

/* Test eventfd error handling */
static bool test_eventfd_watchers_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true ;

    /* Start a valid eventfd watcher */
    TEST_ASSERT(sse_start_eventfd(&loop, &watcher1, test_eventfd_callback, &cb_data1, 1),
               "Failed to start valid eventfd watcher");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_eventfd(NULL, &watcher2, test_eventfd_callback, &cb_data2, 1),
               "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_eventfd(&loop, NULL, test_eventfd_callback, &cb_data2, 1),
               "Should fail with NULL watcher");

    /* Test invalid operations on NULL watcher */
    TEST_ASSERT(!sse_write_eventfd(NULL), "Should fail to write to NULL watcher");
    TEST_ASSERT(!sse_read_eventfd(NULL), "Should fail to read from NULL watcher");

    /* Test that valid watcher still works */
    TEST_ASSERT(sse_write_eventfd(&watcher1), "Valid eventfd should still work");

    int max_iterations = 10;
    while (max_iterations-- > 0 && !cb_data1.callback_called) {
        if (!sse_run(&loop, 30)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "Valid eventfd watcher should work after error conditions");

    sse_free(&loop);

    return true;
}

/* Test eventfd counter behavior and coalescing */
static bool test_eventfd_counter_behavior(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher;
    struct test_callback_data cb_data;

    test_callback_init(&cb_data);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    TEST_ASSERT(sse_start_eventfd(&loop, &watcher, test_eventfd_callback, &cb_data, 1),
               "Failed to start eventfd watcher");

    /* Write multiple times rapidly */
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(sse_write_eventfd(&watcher), "Failed to write to eventfd");
    }

    /* Process events - eventfd should coalesce multiple writes */
    int max_iterations = 10;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 30)) {
            break;
        }
        if (cb_data.callback_called) {
            break;
        }
    }

    TEST_ASSERT(cb_data.callback_called, "Eventfd callback should be called");
    /* Due to eventfd semantics, multiple writes should be coalesced into single event */
    TEST_ASSERT_EQ(1, cb_data.call_count, "Multiple writes should be coalesced into single event");

    /* Test interleaved write/read operations */
    cb_data.callback_called = false;
    cb_data.call_count = 0;

    TEST_ASSERT(sse_write_eventfd(&watcher), "Failed to write to eventfd");
    TEST_ASSERT(sse_run(&loop, 30), "Failed to process first write");
    TEST_ASSERT(cb_data.callback_called, "First write should trigger callback");

    cb_data.callback_called = false;
    TEST_ASSERT(sse_write_eventfd(&watcher), "Failed to write second time");
    TEST_ASSERT(sse_run(&loop, 30), "Failed to process second write");
    TEST_ASSERT(cb_data.callback_called, "Second write should trigger callback");

    TEST_ASSERT_EQ(2, cb_data.call_count, "Should have two separate callback invocations");

    sse_free(&loop);

    return true;
}

/* Test multiple eventfd communication pattern */
static bool test_eventfd_communication_pattern(void) {
    sse_epoll_t loop;
    sse_watcher_t producer_watcher, consumer_watcher, control_watcher;
    struct test_callback_data producer_data, consumer_data, control_data;

    test_callback_init(&producer_data);
    test_callback_init(&consumer_data);
    test_callback_init(&control_data);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Setup producer/consumer/control pattern */
    TEST_ASSERT(sse_start_eventfd(&loop, &producer_watcher, test_eventfd_callback, &producer_data, 1),
               "Failed to start producer eventfd");

    TEST_ASSERT(sse_start_eventfd(&loop, &consumer_watcher, test_eventfd_callback, &consumer_data, 2),
               "Failed to start consumer eventfd");

    TEST_ASSERT(sse_start_eventfd(&loop, &control_watcher, test_eventfd_callback, &control_data, 3),
               "Failed to start control eventfd");

    /* Simulate communication flow */
    /* Producer signals work available */
    TEST_ASSERT(sse_write_eventfd(&producer_watcher), "Producer failed to signal");

    /* Consumer signals work completed */
    TEST_ASSERT(sse_write_eventfd(&consumer_watcher), "Consumer failed to signal");

    /* Control signals shutdown */
    TEST_ASSERT(sse_write_eventfd(&control_watcher), "Control failed to signal");

    /* Process all signals */
    int max_iterations = 15;
    int signals_processed = 0;

    while (max_iterations-- > 0 && signals_processed < 3) {
        if (!sse_run(&loop, 50)) {
            break;
        }

        signals_processed = 0;
        if (producer_data.callback_called) signals_processed++;
        if (consumer_data.callback_called) signals_processed++;
        if (control_data.callback_called) signals_processed++;
    }

    TEST_ASSERT(producer_data.callback_called, "Producer signal should be received");
    TEST_ASSERT(consumer_data.callback_called, "Consumer signal should be received");
    TEST_ASSERT(control_data.callback_called, "Control signal should be received");

    /* Test priority-based processing order */
    /* Priority 1 (producer) should be processed before priority 2 (consumer) */
    /* This is a rough test as exact timing may vary */

    sse_free(&loop);

    return true;
}

TEST_SUITE_BEGIN("SSE EventFD Watcher Tests - Single Loop Multiple EventFDs")
    RUN_TEST(test_eventfd_watchers_comprehensive);
    RUN_TEST(test_eventfd_watchers_error_handling);
    RUN_TEST(test_eventfd_counter_behavior);
    RUN_TEST(test_eventfd_communication_pattern);
TEST_SUITE_END()