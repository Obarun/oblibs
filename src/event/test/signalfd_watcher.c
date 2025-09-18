/*
 * signalfd_watcher.c
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
 */

#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <oblibs/sse.h>
#include "framework.h"

/* Test callback for signal events */
static void test_signal_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store signal info */
    if (w->sdata) {
        sse_signal_t *signal_data = (sse_signal_t *)w->sdata;
        data->user_data = (void *)(intptr_t)signal_data->si.ssi_signo;
    }
}

/* Main signal watcher test - single watcher, multiple signals */
static bool test_signal_watchers_comprehensive(void) {
    sse_epoll_t loop;
    sse_watcher_t signal_watcher;  // Only ONE watcher
    struct test_callback_data signal_data;

    test_callback_init(&signal_data);

    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true;

    /* === Start single signal watcher === */
    TEST_ASSERT(sse_start_signal(&loop, &signal_watcher, test_signal_callback, &signal_data, 1),
               "Failed to start signal watcher");

    /* === Attach multiple signals to the same watcher === */
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGUSR1), "Failed to attach SIGUSR1");
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGUSR2), "Failed to attach SIGUSR2");
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGTERM), "Failed to attach SIGTERM");
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGINT), "Failed to attach SIGINT");

    /* === Send signals === */
    TEST_ASSERT(kill(getpid(), SIGUSR1) == 0, "Failed to send SIGUSR1");
    TEST_ASSERT(kill(getpid(), SIGUSR2) == 0, "Failed to send SIGUSR2");
    TEST_ASSERT(kill(getpid(), SIGTERM) == 0, "Failed to send SIGTERM");

    /* Process signals - all come through the same watcher */
    int max_iterations = 20;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 50)) {
            break;
        }
    }

    TEST_ASSERT(signal_data.callback_called, "Signal callback should have been called");
    TEST_ASSERT(signal_data.call_count >= 1, "Should receive signals");

    sse_free(&loop);
    return true;
}

/* Test signal watcher error handling */
static bool test_signal_watchers_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true ;

    /* Start a valid signal watcher */
    TEST_ASSERT(sse_start_signal(&loop, &watcher1, test_signal_callback, &cb_data1, 1),
               "Failed to start valid signal watcher");

    TEST_ASSERT(sse_attach_signal(&watcher1, SIGUSR1), "Failed to attach SIGUSR1");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_signal(NULL, &watcher2, test_signal_callback, &cb_data2, 1),
               "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_signal(&loop, NULL, test_signal_callback, &cb_data2, 1),
               "Should fail with NULL watcher");

    /* Test invalid signal attachment */
    TEST_ASSERT(!sse_attach_signal(&watcher1, -1), "Should fail with invalid signal number");
    TEST_ASSERT(!sse_attach_signal(&watcher1, 999), "Should fail with invalid signal number");

    /* Test that valid watcher still works */
    kill(getpid(), SIGUSR1);

    int max_iterations = 10;
    while (max_iterations-- > 0 && !cb_data1.callback_called) {
        if (!sse_run(&loop, 30)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "Valid signal watcher should still work after errors");

    sse_free(&loop);

    return true;
}

/* Test signal watcher lifecycle and cleanup */
static bool test_signal_watchers_lifecycle(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher;
    struct test_callback_data data, data2, data3;

    test_callback_init(&data);
    test_callback_init(&data2);
    test_callback_init(&data3);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Start multiple signal watchers */
    TEST_ASSERT(sse_start_signal(&loop, &watcher, test_signal_callback, &data, 1),
               "Failed to start first signal watcher");
    TEST_ASSERT(sse_attach_signal(&watcher, SIGUSR1), "Failed to attach SIGUSR1 to watcher");
    TEST_ASSERT(sse_attach_signal(&watcher, SIGUSR2), "Failed to attach SIGUSR2 to watcher");
    TEST_ASSERT(sse_attach_signal(&watcher, SIGTERM), "Failed to attach SIGTERM to watcher");

    TEST_ASSERT(sse_watcher_active(&watcher), "First watcher should still be active");

    /* Send signals to test selective behavior */
    kill(getpid(), SIGUSR1); /* Should be received by watcher */
    kill(getpid(), SIGUSR2); /* Should NOT be received (watcher2 stopped) */
    kill(getpid(), SIGTERM); /* Should be received by watcher3 */

    int max_iterations = 15;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 50)) {
            break;
        }
        if (data.callback_called && data3.callback_called) {
            break;
        }
    }

    TEST_ASSERT(data.callback_called, "Active watcher should receive SIGUSR1");

    /* Test restart functionality */
    TEST_ASSERT(sse_stop_signal(&watcher), "Failed to restart watcher");
    TEST_ASSERT(sse_restart_signal(&watcher), "Failed to restart watcher");
    TEST_ASSERT(sse_watcher_active(&watcher), "Restarted watcher should be active");

    /* Send SIGUSR2 again - should now be received */
    data.callback_called = false; /* Reset */
    kill(getpid(), SIGUSR2);

    max_iterations = 10;
    while (max_iterations-- > 0 && !data.callback_called) {
        if (!sse_run(&loop, 50)) {
            break;
        }
    }

    TEST_ASSERT(data.callback_called, "Restarted watcher2 should receive SIGUSR2");

    sse_free(&loop);

    return true;
}

TEST_SUITE_BEGIN("SSE Signal Watcher Tests - Single Loop Multiple Signals")
    RUN_TEST(test_signal_watchers_comprehensive);
    RUN_TEST(test_signal_watchers_error_handling);
    RUN_TEST(test_signal_watchers_lifecycle);
TEST_SUITE_END()