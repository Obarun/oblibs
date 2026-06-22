/*
 * child_watcher.c
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

#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include <oblibs/sse.h>
#include "framework.h"

/* Test callback for child events */
static void test_child_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store child status and watcher info */
    if (w->sdata) {
        sse_child_t *child_data = (sse_child_t *)w->sdata;
        data->user_data = (void *)(intptr_t)child_data->status;
    }
}

/* Child process that exits immediately with code 42 */
static void quick_exit_child(void) {
    exit(42);
}

/* Child process that sleeps then exits with code 24 */
static void slow_exit_child(void) {
    usleep(200000); /* 200ms */
    exit(24);
}

/* Child process that runs indefinitely */
static void infinite_child(void) {
    while (1) {
        usleep(50000); /* 50ms */
    }
}

/* Child that exits with code 100 after short delay */
static void delayed_exit_child(void) {
    usleep(150000); /* 150ms */
    exit(100);
}

/* Main child watcher test - single loop handling multiple children */
static bool test_child_watchers_comprehensive(void) {
    sse_epoll_t loop;

    /* Multiple child watchers */
    sse_watcher_t quick_watcher, slow_watcher, delayed_watcher, infinite_watcher;
    struct test_callback_data quick_data, slow_data, delayed_data, infinite_data;

    /* Child PIDs */
    pid_t quick_pid, slow_pid, delayed_pid, infinite_pid;

    /* Initialize callback data */
    test_callback_init(&quick_data);
    test_callback_init(&slow_data);
    test_callback_init(&delayed_data);
    test_callback_init(&infinite_data);

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true ;

    /* === Spawn children === */
    quick_pid = test_spawn_child(quick_exit_child);
    TEST_ASSERT(quick_pid > 0, "Failed to spawn quick exit child");

    slow_pid = test_spawn_child(slow_exit_child);
    TEST_ASSERT(slow_pid > 0, "Failed to spawn slow exit child");

    delayed_pid = test_spawn_child(delayed_exit_child);
    TEST_ASSERT(delayed_pid > 0, "Failed to spawn delayed exit child");

    infinite_pid = test_spawn_child(infinite_child);
    TEST_ASSERT(infinite_pid > 0, "Failed to spawn infinite child");

    /* === Start watchers with different priorities === */
    TEST_ASSERT(sse_start_child(&loop, &quick_watcher, test_child_callback, &quick_data,
                               quick_pid, 1, false),
               "Failed to start quick child watcher");

    TEST_ASSERT(sse_start_child(&loop, &slow_watcher, test_child_callback, &slow_data,
                               slow_pid, 2, false),
               "Failed to start slow child watcher");

    TEST_ASSERT(sse_start_child(&loop, &delayed_watcher, test_child_callback, &delayed_data,
                               delayed_pid, 3, false),
               "Failed to start delayed child watcher");

    TEST_ASSERT(sse_start_child(&loop, &infinite_watcher, test_child_callback, &infinite_data,
                               infinite_pid, 4, false),
               "Failed to start infinite child watcher");

    /* === Verify all watchers are active === */
    TEST_ASSERT(sse_watcher_active(&quick_watcher), "Quick child watcher should be active");
    TEST_ASSERT(sse_watcher_active(&slow_watcher), "Slow child watcher should be active");
    TEST_ASSERT(sse_watcher_active(&delayed_watcher), "Delayed child watcher should be active");
    TEST_ASSERT(sse_watcher_active(&infinite_watcher), "Infinite child watcher should be active");

    /* === Verify watcher setup === */
    TEST_ASSERT_EQ(SSE_TYPE_CHILD, quick_watcher.type, "Wrong watcher type");
    TEST_ASSERT_NOT_NULL(quick_watcher.sdata, "Child watcher should have sdata");

    sse_child_t *child_data = (sse_child_t *)quick_watcher.sdata;
    TEST_ASSERT_EQ(quick_pid, child_data->pid, "Wrong PID stored in watcher");

    /* === Process events - wait for child exits === */
    int max_iterations = 30;  /* Allow more time for all children to exit */
    int finished_children = 0;

    while (max_iterations-- > 0 && finished_children < 3) {
        if (!sse_run(&loop, 100)) {
            break;
        }

        /* Count finished children (excluding infinite child) */
        finished_children = 0;
        if (quick_data.callback_called) finished_children++;
        if (slow_data.callback_called) finished_children++;
        if (delayed_data.callback_called) finished_children++;

        if (finished_children >= 3) break;
    }

    /* === Verify child exits were detected === */
    TEST_ASSERT(quick_data.callback_called, "Quick child callback should have been called");
    TEST_ASSERT(slow_data.callback_called, "Slow child callback should have been called");
    TEST_ASSERT(delayed_data.callback_called, "Delayed child callback should have been called");

    /* Infinite child should still be running */
    TEST_ASSERT(!infinite_data.callback_called, "Infinite child should still be running");

    /* === Verify exit codes === */
    int quick_status = (int)(intptr_t)quick_data.user_data;
    int slow_status = (int)(intptr_t)slow_data.user_data;
    int delayed_status = (int)(intptr_t)delayed_data.user_data;

    TEST_ASSERT(WIFEXITED(quick_status), "Quick child should have exited normally");
    TEST_ASSERT_EQ(42, WEXITSTATUS(quick_status), "Quick child should have exit code 42");

    TEST_ASSERT(WIFEXITED(slow_status), "Slow child should have exited normally");
    TEST_ASSERT_EQ(24, WEXITSTATUS(slow_status), "Slow child should have exit code 24");

    TEST_ASSERT(WIFEXITED(delayed_status), "Delayed child should have exited normally");
    TEST_ASSERT_EQ(100, WEXITSTATUS(delayed_status), "Delayed child should have exit code 100");

    /* === Test termination by signal === */
    TEST_ASSERT(kill(infinite_pid, SIGTERM) == 0, "Failed to send SIGTERM to infinite child");

    /* Wait for signal termination */
    max_iterations = 10;
    while (max_iterations-- > 0 && !infinite_data.callback_called) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(infinite_data.callback_called, "Infinite child termination should be detected");

    int infinite_status = (int)(intptr_t)infinite_data.user_data;
    TEST_ASSERT(WIFSIGNALED(infinite_status), "Infinite child should be terminated by signal");
    TEST_ASSERT_EQ(SIGTERM, WTERMSIG(infinite_status), "Should be terminated by SIGTERM");

    /* === Test watcher state management === */
    /* Try to stop a watcher for an already-dead process */
    TEST_ASSERT(sse_stop_child(&quick_watcher), "Should be able to stop dead child watcher");
    TEST_ASSERT(!sse_watcher_active(&quick_watcher), "Dead child watcher should be inactive");

    /* === Cleanup === */
    sse_free(&loop);

    return true;
}

/* Test child watcher error handling and edge cases */
static bool test_child_watchers_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;
    pid_t child_pid;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true ;

    child_pid = test_spawn_child(quick_exit_child);
    TEST_ASSERT(child_pid > 0, "Failed to spawn child");

    /* Start first watcher successfully */
    TEST_ASSERT(sse_start_child(&loop, &watcher1, test_child_callback, &cb_data1,
                               child_pid, 1, false),
               "Failed to start child watcher");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_child(NULL, &watcher2, test_child_callback, &cb_data2, 999, 1, false),
               "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_child(&loop, NULL, test_child_callback, &cb_data2, 999, 1, false),
               "Should fail with NULL watcher");

    TEST_ASSERT(!sse_start_child(&loop, &watcher2, test_child_callback, &cb_data2, 0, 1, false),
               "Should fail with invalid PID (0)");

    TEST_ASSERT(!sse_start_child(&loop, &watcher2, test_child_callback, &cb_data2, -1, 1, false),
               "Should fail with invalid PID (-1)");

    /* Test that first watcher still works */
    int max_iterations = 10;
    while (max_iterations-- > 0 && !cb_data1.callback_called) {
        if (!sse_run(&loop, 50)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "Valid child watcher should still work after errors");

    sse_free(&loop);

    return true;
}

/* Test child watcher lifecycle management */
static bool test_child_watchers_lifecycle(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;
    pid_t child_pid1, child_pid2;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Start two long-running children */
    child_pid1 = test_spawn_child(infinite_child);
    child_pid2 = test_spawn_child(infinite_child);
    TEST_ASSERT(child_pid1 > 0, "Failed to spawn first child");
    TEST_ASSERT(child_pid2 > 0, "Failed to spawn second child");

    TEST_ASSERT(sse_start_child(&loop, &watcher1, test_child_callback, &cb_data1,
                               child_pid1, 1, false),
               "Failed to start first child watcher");

    TEST_ASSERT(sse_start_child(&loop, &watcher2, test_child_callback, &cb_data2,
                               child_pid2, 2, false),
               "Failed to start second child watcher");

    /* Test stop/restart operations */
    TEST_ASSERT(sse_stop_child(&watcher1), "Failed to stop first child watcher");
    TEST_ASSERT(!sse_watcher_active(&watcher1), "First watcher should be inactive");
    TEST_ASSERT(sse_watcher_active(&watcher2), "Second watcher should still be active");

    /* Restart first watcher */
    TEST_ASSERT(sse_restart_child(&watcher1), "Failed to restart first child watcher");
    TEST_ASSERT(sse_watcher_active(&watcher1), "First watcher should be active again");

    /* Kill children and verify detection */
    TEST_ASSERT(kill(child_pid1, SIGKILL) == 0, "Failed to kill first child");
    TEST_ASSERT(kill(child_pid2, SIGUSR1) == 0, "Failed to signal second child");

    /* Wait for termination events */
    int max_iterations = 15;
    while (max_iterations-- > 0 && (!cb_data1.callback_called || !cb_data2.callback_called)) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "First child termination should be detected");
    TEST_ASSERT(cb_data2.callback_called, "Second child termination should be detected");

    /* Verify signal information */
    int status1 = (int)(intptr_t)cb_data1.user_data;
    int status2 = (int)(intptr_t)cb_data2.user_data;

    TEST_ASSERT(WIFSIGNALED(status1), "First child should be terminated by signal");
    TEST_ASSERT_EQ(SIGKILL, WTERMSIG(status1), "First child should be terminated by SIGKILL");

    TEST_ASSERT(WIFSIGNALED(status2), "Second child should be terminated by signal");
    TEST_ASSERT_EQ(SIGUSR1, WTERMSIG(status2), "Second child should be terminated by SIGUSR1");

    sse_free(&loop);

    return true;
}

TEST_SUITE_BEGIN("SSE Child Watcher Tests - Single Loop Multiple Children")
    RUN_TEST(test_child_watchers_comprehensive);
    RUN_TEST(test_child_watchers_error_handling);
    RUN_TEST(test_child_watchers_lifecycle);
TEST_SUITE_END()