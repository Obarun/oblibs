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
 * except according to the terms contained in the LICENSE file.
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

/* File-local recorder: captures the signo carried by the dispatched siginfo so
 * a test can assert WHICH signal was delivered, not merely that some callback
 * ran. sse_dispatch_signal coalesces (keeps the last siginfo) and calls the
 * callback once, so g_last_signo is that last signal's number. */
static int g_last_signo = 0 ;
static int g_signal_calls = 0 ;

static void record_signal_cb(sse_watcher_t *w, void *cbdata, int event) {
    (void) cbdata ; (void) event ;
    if (w->sdata) {
        sse_signal_t *s = (sse_signal_t *)w->sdata ;
        g_last_signo = (int)s->si.ssi_signo ;
        g_signal_calls++ ;
    }
}

/* Drain the loop until the recorder fires or the iteration budget runs out. */
static void pump_until_signal(sse_epoll_t *loop) {
    int it = 20 ;
    while (it-- > 0 && !g_signal_calls) {
        if (!sse_run(loop, 50))
            break ;
    }
}

/* Main signal watcher test - one watcher, several attached signals. Sent one at
 * a time so the coalesced siginfo is deterministic: each attached signal must
 * come back carrying its own ssi_signo. */
static bool test_signal_watchers_comprehensive(void) {
    sse_epoll_t loop;
    sse_watcher_t signal_watcher;

    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true;

    TEST_ASSERT(sse_start_signal(&loop, &signal_watcher, record_signal_cb, NULL, 1),
               "Failed to start signal watcher");

    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGUSR1), "Failed to attach SIGUSR1");
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGUSR2), "Failed to attach SIGUSR2");
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGTERM), "Failed to attach SIGTERM");

    int sigs[] = { SIGUSR1, SIGUSR2, SIGTERM };
    for (size_t i = 0 ; i < sizeof(sigs) / sizeof(sigs[0]) ; i++) {
        g_last_signo = 0 ; g_signal_calls = 0 ;
        TEST_ASSERT(kill(getpid(), sigs[i]) == 0, "Failed to send signal");
        pump_until_signal(&loop);
        TEST_ASSERT(g_signal_calls >= 1, "Signal callback should have fired");
        TEST_ASSERT(g_last_signo == sigs[i], "Dispatched siginfo must carry the signal that was sent");
    }

    sse_free(&loop);
    return true;
}

/* A burst of the same standard signal collapses to a single pending instance;
 * the delivery must still surface and carry the right signo. */
static bool test_signal_coalescing(void) {
    sse_epoll_t loop;
    sse_watcher_t w;

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true;

    TEST_ASSERT(sse_start_signal(&loop, &w, record_signal_cb, NULL, 1), "Failed to start signal watcher");
    TEST_ASSERT(sse_attach_signal(&w, SIGUSR1), "Failed to attach SIGUSR1");

    g_last_signo = 0 ; g_signal_calls = 0 ;
    for (int i = 0 ; i < 5 ; i++)
        kill(getpid(), SIGUSR1);
    pump_until_signal(&loop);
    TEST_ASSERT(g_signal_calls >= 1, "Coalesced burst should still fire the callback");
    TEST_ASSERT(g_last_signo == SIGUSR1, "Coalesced delivery must carry SIGUSR1");

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

/* Test signal watcher lifecycle: deliver, stop, restart, deliver again. */
static bool test_signal_watchers_lifecycle(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher;

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    TEST_ASSERT(sse_start_signal(&loop, &watcher, record_signal_cb, NULL, 1),
               "Failed to start signal watcher");
    TEST_ASSERT(sse_attach_signal(&watcher, SIGUSR1), "Failed to attach SIGUSR1 to watcher");
    TEST_ASSERT(sse_attach_signal(&watcher, SIGUSR2), "Failed to attach SIGUSR2 to watcher");
    TEST_ASSERT(sse_watcher_active(&watcher), "Watcher should be active");

    /* Delivered while active, carrying the right signo. */
    g_last_signo = 0 ; g_signal_calls = 0 ;
    kill(getpid(), SIGUSR1);
    pump_until_signal(&loop);
    TEST_ASSERT(g_signal_calls >= 1, "Active watcher should receive SIGUSR1");
    TEST_ASSERT(g_last_signo == SIGUSR1, "Active watcher must receive SIGUSR1");

    /* Stop then restart. */
    TEST_ASSERT(sse_stop_signal(&watcher), "Failed to stop watcher");
    TEST_ASSERT(sse_restart_signal(&watcher), "Failed to restart watcher");
    TEST_ASSERT(sse_watcher_active(&watcher), "Restarted watcher should be active");

    /* A signal sent after restart must still be delivered with its signo. */
    g_last_signo = 0 ; g_signal_calls = 0 ;
    kill(getpid(), SIGUSR2);
    pump_until_signal(&loop);
    TEST_ASSERT(g_signal_calls >= 1, "Restarted watcher should receive SIGUSR2");
    TEST_ASSERT(g_last_signo == SIGUSR2, "Restarted watcher must receive SIGUSR2");

    sse_free(&loop);

    return true;
}

TEST_SUITE_BEGIN("SSE Signal Watcher Tests - Single Loop Multiple Signals")
    RUN_TEST(test_signal_watchers_comprehensive);
    RUN_TEST(test_signal_coalescing);
    RUN_TEST(test_signal_watchers_error_handling);
    RUN_TEST(test_signal_watchers_lifecycle);
TEST_SUITE_END()