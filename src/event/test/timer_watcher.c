/*
 * timer_watcher.c
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


#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <oblibs/sse.h>
#include "framework.h"

/* Test callback for timer events */
static void test_timer_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store watcher pointer for identification */
    data->user_data = w;
}

/* Helper to get current time in milliseconds */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/* Main timer watcher test - single loop handling multiple timers */
static bool test_timer_watchers_comprehensive(void) {
    sse_epoll_t loop;

    /* Multiple timer watchers */
    sse_watcher_t oneshot_watcher, periodic_watcher, long_watcher, quick_watcher;
    struct test_callback_data oneshot_data, periodic_data, long_data, quick_data;

    /* Initialize callback data */
    test_callback_init(&oneshot_data);
    test_callback_init(&periodic_data);
    test_callback_init(&long_data);
    test_callback_init(&quick_data);

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true ;

    /* === Start different types of timers === */

    /* One-shot timer - 100ms */
    TEST_ASSERT(sse_start_timer(&loop, &oneshot_watcher, test_timer_callback, &oneshot_data,
                               100, 0, 1),
               "Failed to start one-shot timer");

    /* Periodic timer - 50ms interval, 50ms initial delay */
    TEST_ASSERT(sse_start_timer(&loop, &periodic_watcher, test_timer_callback, &periodic_data,
                               25, 50, 2),
               "Failed to start periodic timer");

    /* Long timer - 300ms */
    TEST_ASSERT(sse_start_timer(&loop, &long_watcher, test_timer_callback, &long_data,
                               300, 0, 3),
               "Failed to start long timer");

    /* Quick timer - 25ms */
    TEST_ASSERT(sse_start_timer(&loop, &quick_watcher, test_timer_callback, &quick_data,
                               25, 0, 4),
               "Failed to start quick timer");

    /* === Verify all watchers are active === */
    TEST_ASSERT(sse_watcher_active(&oneshot_watcher), "One-shot timer should be active");
    TEST_ASSERT(sse_watcher_active(&periodic_watcher), "Periodic timer should be active");
    TEST_ASSERT(sse_watcher_active(&long_watcher), "Long timer should be active");
    TEST_ASSERT(sse_watcher_active(&quick_watcher), "Quick timer should be active");

    /* === Verify timer setup === */
    TEST_ASSERT_EQ(SSE_TYPE_TIMER, oneshot_watcher.type, "Wrong watcher type");
    TEST_ASSERT_NOT_NULL(oneshot_watcher.sdata, "Timer should have sdata");

    sse_timer_t *timer_data = (sse_timer_t *)oneshot_watcher.sdata;
    TEST_ASSERT_EQ(0, timer_data->timeout.it_value.tv_sec, "One-shot timer seconds");
    TEST_ASSERT_EQ(100000000, timer_data->timeout.it_value.tv_nsec, "One-shot timer nanoseconds");
    TEST_ASSERT_EQ(0, timer_data->timeout.it_interval.tv_sec, "One-shot should have no interval");

    /* === Process timer events === */
    uint64_t start_time = get_time_ms();
    int max_iterations = 50;
    int periodic_events = 0;

    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 20)) {
            break;
        }

        uint64_t current_time = get_time_ms();

        /* Count periodic timer events */
        if (periodic_data.call_count > periodic_events) {
            periodic_events = periodic_data.call_count;
        }

        /* Break if we've run long enough and seen expected events */
        if ((current_time - start_time) > 350 &&
            quick_data.callback_called &&
            oneshot_data.callback_called &&
            periodic_events >= 2) {
            break;
        }
    }

    /* === Verify timer events === */
    TEST_ASSERT(quick_data.callback_called, "Quick timer (25ms) should have fired");
    TEST_ASSERT_EQ(1, quick_data.call_count, "Quick timer should fire once");
    TEST_ASSERT(quick_data.last_event & SSE_READ, "Timer should generate read event");

    TEST_ASSERT(oneshot_data.callback_called, "One-shot timer (100ms) should have fired");
    TEST_ASSERT_EQ(1, oneshot_data.call_count, "One-shot timer should fire exactly once");

    TEST_ASSERT(periodic_data.callback_called, "Periodic timer should have fired");
    TEST_ASSERT(periodic_data.call_count >= 2, "Periodic timer should fire multiple times");

    TEST_ASSERT(long_data.callback_called, "Long timer (300ms) should have fired");
    TEST_ASSERT_EQ(1, long_data.call_count, "Long timer should fire once");

    /* === Test timer modification === */
    /* Modify periodic timer to have different interval */
    TEST_ASSERT(sse_modify_timer(&periodic_watcher, 200, 100),
               "Failed to modify periodic timer");

    /* Reset periodic counter and test new timing */
    int old_periodic_count = periodic_data.call_count;
    test_sleep_ms(250); /* Wait for new timing */

    max_iterations = 10;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 30)) {
            break;
        }
        if (periodic_data.call_count > old_periodic_count + 1) {
            break;
        }
    }

    TEST_ASSERT(periodic_data.call_count > old_periodic_count,
               "Modified periodic timer should continue firing");

    /* === Test timer stop/restart === */
    TEST_ASSERT(sse_stop_timer(&long_watcher), "Failed to stop long timer");
    TEST_ASSERT(!sse_watcher_active(&long_watcher), "Stopped timer should be inactive");

    TEST_ASSERT(sse_restart_timer(&long_watcher), "Failed to restart timer");
    TEST_ASSERT(sse_watcher_active(&long_watcher), "Restarted timer should be active");

    sse_free(&loop);

    return true;
}

/* Test timer precision and ordering */
static bool test_timer_precision_and_ordering(void) {
    sse_epoll_t loop;
    sse_watcher_t timer1, timer2, timer3;
    struct test_callback_data data1, data2, data3;
    uint64_t start_time, event_times[3] = {0};
    int event_order[3] = {-1, -1, -1};
    int event_count = 0;

    test_callback_init(&data1);
    test_callback_init(&data2);
    test_callback_init(&data3);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Start timers with different delays and priorities */
    TEST_ASSERT(sse_start_timer(&loop, &timer1, test_timer_callback, &data1,
                               60, 0, 1), /* 60ms, high priority */
               "Failed to start timer1");

    TEST_ASSERT(sse_start_timer(&loop, &timer2, test_timer_callback, &data2,
                               40, 0, 3), /* 40ms, low priority */
               "Failed to start timer2");

    TEST_ASSERT(sse_start_timer(&loop, &timer3, test_timer_callback, &data3,
                               80, 0, 2), /* 80ms, medium priority */
               "Failed to start timer3");

    start_time = get_time_ms();
    int max_iterations = 30;

    while (max_iterations-- > 0 && event_count < 3) {
        if (!sse_run(&loop, 20)) {
            break;
        }

        /* Record event times and order */
        if (data2.callback_called && event_times[0] == 0) {
            event_times[0] = get_time_ms() - start_time;
            event_order[event_count++] = 2; /* timer2 */
        }
        if (data1.callback_called && event_times[1] == 0) {
            event_times[1] = get_time_ms() - start_time;
            event_order[event_count++] = 1; /* timer1 */
        }
        if (data3.callback_called && event_times[2] == 0) {
            event_times[2] = get_time_ms() - start_time;
            event_order[event_count++] = 3; /* timer3 */
        }
    }

    /* Verify timing accuracy (within reasonable tolerance) */
    TEST_ASSERT(event_times[0] >= 35 && event_times[0] <= 65,
               "Timer2 (40ms) timing should be accurate");
    TEST_ASSERT(event_times[1] >= 55 && event_times[1] <= 85,
               "Timer1 (60ms) timing should be accurate");
    TEST_ASSERT(event_times[2] >= 75 && event_times[2] <= 105,
               "Timer3 (80ms) timing should be accurate");

    /* Verify chronological order */
    TEST_ASSERT(event_order[0] == 2, "Timer2 (40ms) should fire first");
    TEST_ASSERT(event_order[1] == 1, "Timer1 (60ms) should fire second");
    TEST_ASSERT(event_order[2] == 3, "Timer3 (80ms) should fire third");

    sse_free(&loop);

    return true;
}

/* Test timer error handling and edge cases */
static bool test_timer_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true ;

    /* Start a valid timer */
    TEST_ASSERT(sse_start_timer(&loop, &watcher1, test_timer_callback, &cb_data1,
                               100, 0, 1),
               "Failed to start valid timer");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_timer(NULL, &watcher2, test_timer_callback, &cb_data2, 100, 0, 1),
               "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_timer(&loop, NULL, test_timer_callback, &cb_data2, 100, 0, 1),
               "Should fail with NULL watcher");

    /* Test zero timeout (should work) */
    TEST_ASSERT(sse_start_timer(&loop, &watcher2, test_timer_callback, &cb_data2, 0, 0, 2),
               "Zero timeout should be valid");

    /* Zero timeout should fire immediately */
    TEST_ASSERT(sse_run(&loop, 10), "Failed to run event loop");
    TEST_ASSERT(cb_data2.callback_called, "Zero timeout timer should fire immediately");

    /* Test that first timer still works */
    int max_iterations = 15;
    while (max_iterations-- > 0 && !cb_data1.callback_called) {
        if (!sse_run(&loop, 20)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "Valid timer should still work after error conditions");

    sse_free(&loop);

    return true;
}

/* Test periodic timer behavior */
static bool test_periodic_timers(void) {
    sse_epoll_t loop;
    sse_watcher_t fast_periodic, slow_periodic;
    struct test_callback_data fast_data, slow_data;
    uint64_t start_time;

    test_callback_init(&fast_data);
    test_callback_init(&slow_data);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Fast periodic: 30ms initial, 30ms period */
    TEST_ASSERT(sse_start_timer(&loop, &fast_periodic, test_timer_callback, &fast_data,
                               30, 30, 1),
               "Failed to start fast periodic timer");

    /* Slow periodic: 80ms initial, 80ms period */
    TEST_ASSERT(sse_start_timer(&loop, &slow_periodic, test_timer_callback, &slow_data,
                               80, 80, 2),
               "Failed to start slow periodic timer");

    start_time = get_time_ms();
    int max_iterations = 60;

    /* Run for about 250ms to see multiple periods */
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 15)) {
            break;
        }

        if ((get_time_ms() - start_time) > 250) {
            break;
        }
    }

    /* Fast timer should have fired multiple times (250ms / 30ms ≈ 8 times) */
    TEST_ASSERT(fast_data.call_count >= 6, "Fast periodic timer should fire multiple times");
    TEST_ASSERT(fast_data.call_count <= 10, "Fast periodic timer shouldn't fire too many times");

    /* Slow timer should have fired fewer times (250ms / 80ms ≈ 3 times) */
    TEST_ASSERT(slow_data.call_count >= 2, "Slow periodic timer should fire at least twice");
    TEST_ASSERT(slow_data.call_count <= 5, "Slow periodic timer shouldn't fire too many times");

    /* Test stopping periodic timer */
    int fast_count_before_stop = fast_data.call_count;
    TEST_ASSERT(sse_stop_timer(&fast_periodic), "Failed to stop fast periodic timer");

    /* Wait a bit more */
    test_sleep_ms(100);
    max_iterations = 10;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 20)) {
            break;
        }
    }

    /* Fast timer should not have fired again */
    TEST_ASSERT_EQ(fast_count_before_stop, fast_data.call_count,
                   "Stopped timer should not fire again");

    /* Slow timer should continue */
    TEST_ASSERT(slow_data.call_count > 3, "Slow periodic timer should continue firing");

    sse_free(&loop);

    return true;
}

TEST_SUITE_BEGIN("SSE Timer Watcher Tests - Single Loop Multiple Timers")
    RUN_TEST(test_timer_watchers_comprehensive);
    RUN_TEST(test_timer_precision_and_ordering);
    RUN_TEST(test_timer_error_handling);
    RUN_TEST(test_periodic_timers);
TEST_SUITE_END()