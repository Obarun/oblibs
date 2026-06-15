/*
 * all_watchers.c
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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <oblibs/sse.h>
#include "framework.h"

/* Test callback that identifies watcher type */
static void test_mixed_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;
    data->user_data = w;

    /* CRITICAL: For I/O watchers, consume the data to clear the ready state */
    if (w->type == SSE_TYPE_IO && (event & SSE_READ)) {
        char buffer[1024];
        ssize_t bytes_read = read(w->fd, buffer, sizeof(buffer));
        (void)bytes_read; // Silence unused variable warning
    }
}

/* Comprehensive test - all watcher types in single loop */
static bool test_all_watchers_comprehensive(void) {
    sse_epoll_t loop;

    /* All watcher types */
    sse_watcher_t io_watcher, child_watcher, timer_watcher, signal_watcher;
    sse_watcher_t eventfd_watcher, inotify_watcher, schedule_watcher;

    /* Callback data for each watcher */
    struct test_callback_data io_data, child_data, timer_data, signal_data;
    struct test_callback_data eventfd_data, inotify_data, schedule_data;

    /* Resources */
    int pipefd[2];
    pid_t child_pid;
    cron_t cron_expr = CRON_EXPR_ZERO;
    const char *test_dir = "/tmp/sse_all_test";

    /* Initialize all callback data */
    test_callback_init(&io_data);
    test_callback_init(&child_data);
    test_callback_init(&timer_data);
    test_callback_init(&signal_data);
    test_callback_init(&eventfd_data);
    test_callback_init(&inotify_data);
    test_callback_init(&schedule_data);

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 50), "Failed to create comprehensive epoll loop");
    loop.running = true;

    /* === Setup child watcher === */
    child_pid = fork();
    if (child_pid == 0) {
        usleep(100); /* 100 µsec */
        sse_free(&loop);
        exit(123);
    }
    TEST_ASSERT(child_pid > 0, "Failed to spawn child process");
    TEST_ASSERT(sse_start_child(&loop, &child_watcher, test_mixed_callback, &child_data,
                               child_pid, 2, false),
               "Failed to start child watcher");

    /* === Setup I/O watcher === */
    TEST_ASSERT(pipe(pipefd) == 0, "Failed to create pipe for I/O test");
    TEST_ASSERT(sse_start_io(&loop, &io_watcher, test_mixed_callback, &io_data,
                            pipefd[0], SSE_READ, 1),
               "Failed to start I/O watcher");

    /* === Setup timer watcher === */
    TEST_ASSERT(sse_start_timer(&loop, &timer_watcher, test_mixed_callback, &timer_data,
                               1, 0, 3), /* 1ms one-shot */
               "Failed to start timer watcher");

    /* === Setup signal watcher === */
    TEST_ASSERT(sse_start_signal(&loop, &signal_watcher, test_mixed_callback, &signal_data, 4),
               "Failed to start signal watcher");
    TEST_ASSERT(sse_attach_signal(&signal_watcher, SIGUSR1), "Failed to attach SIGUSR1");

    /* === Setup eventfd watcher === */
    TEST_ASSERT(sse_start_eventfd(&loop, &eventfd_watcher, test_mixed_callback, &eventfd_data, 5),
               "Failed to start eventfd watcher");

    /* === Setup inotify watcher === */
    system("rm -rf /tmp/sse_all_test");
    TEST_ASSERT(mkdir(test_dir, 0755) == 0, "Failed to create test directory");
    TEST_ASSERT(sse_start_inotify(&loop, &inotify_watcher, test_mixed_callback, &inotify_data, 6),
               "Failed to start inotify watcher");
    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, test_dir, IN_CREATE | IN_DELETE),
               "Failed to attach inotify path");

    /* === Setup schedule watcher === */
    int r = parse_cron("* * * * * ?", &cron_expr, NULL);
    TEST_ASSERT(r, "Failed to create cron expression");
    TEST_ASSERT(sse_start_schedule(&loop, &schedule_watcher, test_mixed_callback, &schedule_data,
                                  &cron_expr, 7),
               "Failed to start schedule watcher");

    /* Modify to fire in 200ms */
    TEST_ASSERT(sse_modify_schedule(&schedule_watcher, time(NULL) + 2),
               "Failed to modify schedule time");

    /* === Verify all watchers are active === */
    TEST_ASSERT(sse_watcher_active(&io_watcher), "I/O watcher should be active");
    TEST_ASSERT(sse_watcher_active(&child_watcher), "Child watcher should be active");
    TEST_ASSERT(sse_watcher_active(&timer_watcher), "Timer watcher should be active");
    TEST_ASSERT(sse_watcher_active(&signal_watcher), "Signal watcher should be active");
    TEST_ASSERT(sse_watcher_active(&eventfd_watcher), "EventFD watcher should be active");
    TEST_ASSERT(sse_watcher_active(&inotify_watcher), "Inotify watcher should be active");
    TEST_ASSERT(sse_watcher_active(&schedule_watcher), "Schedule watcher should be active");

    // /* === Trigger all events === */

    /* Trigger I/O */
    TEST_ASSERT(write(pipefd[1], "test", 4) > 0, "Failed to write to pipe");

    // /* Child will exit automatically */

    // /* Timer will fire automatically */

    /* Trigger signal */
    TEST_ASSERT(kill(getpid(), SIGUSR1) == 0, "Failed to send signal");

    /* Trigger eventfd */
    TEST_ASSERT(sse_write_eventfd(&eventfd_watcher), "Failed to write eventfd");

    /* Trigger inotify */
    int fd = open("/tmp/sse_all_test/trigger_file", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    /* Schedule will fire automatically */

    /* === Process all events === */
    int max_iterations = 500; /* Allow time for all events */
    int events_processed = 0;

    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 200)) {
            break;
        }

        /* Count processed events */
        events_processed = 0;
        if (io_data.callback_called) events_processed++;
        if (child_data.callback_called) events_processed++;
        if (timer_data.callback_called) events_processed++;
        if (signal_data.callback_called) events_processed++;
        if (eventfd_data.callback_called) events_processed++;
        if (inotify_data.callback_called) events_processed++;
        if (schedule_data.callback_called) events_processed++;

        /* Break if we've processed most events */
        if (events_processed >= 7) break;
    }

    /* === Verify all events were processed === */
    TEST_ASSERT(io_data.callback_called, "I/O event should be processed");
    TEST_ASSERT(child_data.callback_called, "Child event should be processed");
    TEST_ASSERT(timer_data.callback_called, "Timer event should be processed");
    TEST_ASSERT(signal_data.callback_called, "Signal event should be processed");
    TEST_ASSERT(eventfd_data.callback_called, "EventFD event should be processed");
    TEST_ASSERT(inotify_data.callback_called, "Inotify event should be processed");
    TEST_ASSERT(schedule_data.callback_called, "Schedule event should be processed");

    /* === Verify watcher types === */
    sse_watcher_t *io_w = (sse_watcher_t *)io_data.user_data;
    sse_watcher_t *child_w = (sse_watcher_t *)child_data.user_data;
    sse_watcher_t *timer_w = (sse_watcher_t *)timer_data.user_data;
    sse_watcher_t *signal_w = (sse_watcher_t *)signal_data.user_data;
    sse_watcher_t *eventfd_w = (sse_watcher_t *)eventfd_data.user_data;
    sse_watcher_t *inotify_w = (sse_watcher_t *)inotify_data.user_data;
    sse_watcher_t *schedule_w = (sse_watcher_t *)schedule_data.user_data;

    TEST_ASSERT_EQ(SSE_TYPE_IO, io_w->type, "I/O watcher type should be correct");
    TEST_ASSERT_EQ(SSE_TYPE_CHILD, child_w->type, "Child watcher type should be correct");
    TEST_ASSERT_EQ(SSE_TYPE_TIMER, timer_w->type, "Timer watcher type should be correct");
    TEST_ASSERT_EQ(SSE_TYPE_SIGNAL, signal_w->type, "Signal watcher type should be correct");
    TEST_ASSERT_EQ(SSE_TYPE_EVENTFD, eventfd_w->type, "EventFD watcher type should be correct");
    TEST_ASSERT_EQ(SSE_TYPE_INOTIFY, inotify_w->type, "Inotify watcher type should be correct");
    TEST_ASSERT_EQ(SSE_TYPE_SCHEDULE, schedule_w->type, "Schedule watcher type should be correct");

    /* === Verify event types === */
    /* I/O is the only watcher whose event flag is discriminating here (READ vs
     * WRITE). The other fd-backed watchers (child/timer/signal/eventfd/inotify/
     * schedule) only ever receive EPOLLIN -> SSE_READ from epoll, so asserting
     * SSE_READ on them is a tautology that proves nothing; their real per-type
     * effects are covered by the dedicated *_watcher tests. */
    TEST_ASSERT(io_data.last_event & SSE_READ, "I/O should generate read event");

    /* === Cleanup === */
    sse_free(&loop);
    close(pipefd[1]);
    system("rm -rf /tmp/sse_all_test");

    return true;
}

/* Test mixed watcher lifecycle operations */
static bool test_mixed_watcher_lifecycle(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2, watcher3;
    struct test_callback_data data1, data2, data3;
    int pipefd[2];

    test_callback_init(&data1);
    test_callback_init(&data2);
    test_callback_init(&data3);

    TEST_ASSERT(sse_new(&loop, 20), "Failed to create loop for lifecycle test");
    loop.running = true ;

    /* Setup mixed watchers */
    TEST_ASSERT(pipe(pipefd) == 0, "Failed to create first pipe");

    TEST_ASSERT(sse_start_io(&loop, &watcher1, test_mixed_callback, &data1,
                            pipefd[0], SSE_READ, 1),
               "Failed to start first I/O watcher");

    TEST_ASSERT(sse_start_timer(&loop, &watcher2, test_mixed_callback, &data2,
                               100, 50, 2), /* 100ms initial, 50ms period */
               "Failed to start timer watcher");

    TEST_ASSERT(sse_start_eventfd(&loop, &watcher3, test_mixed_callback, &data3, 3),
               "Failed to start eventfd watcher");

    /* Test selective stop/start operations */
    TEST_ASSERT(sse_stop_timer(&watcher2), "Failed to stop timer");
    TEST_ASSERT(!sse_watcher_active(&watcher2), "Timer should be inactive");
    TEST_ASSERT(sse_watcher_active(&watcher1), "I/O should still be active");
    TEST_ASSERT(sse_watcher_active(&watcher3), "EventFD should still be active");

    /* Trigger remaining active watchers */
    write(pipefd[1], "test", 4);
    sse_write_eventfd(&watcher3);

    int max_iterations = 20;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 50)) {
            break;
        }
    }

    TEST_ASSERT(data1.callback_called, "Active I/O watcher should fire");
    TEST_ASSERT(!data2.callback_called, "Stopped timer should not fire");
    TEST_ASSERT(data3.callback_called, "Active EventFD watcher should fire");

    /* Restart timer and test */
    TEST_ASSERT(sse_restart_timer(&watcher2), "Failed to restart timer");
    TEST_ASSERT(sse_watcher_active(&watcher2), "Timer should be active again");

    data2.callback_called = false;
    max_iterations = 25;
    while (max_iterations-- > 0 && !data2.callback_called) {
        if (!sse_run(&loop, 50)) {
            break;
        }
    }

    TEST_ASSERT(data2.callback_called, "Restarted timer should fire");

    sse_free(&loop);
    close(pipefd[1]);

    return true;
}

/* Mass registration, dispatch, and cleanup: 15 watchers of three types must all
 * be active, all actually fire (not merely register), and then be torn down by a
 * single sse_free() without leak (proven under the sanitizer build). */
static bool test_mixed_watcher_cleanup(void) {
    sse_epoll_t loop;

    TEST_ASSERT(sse_new(&loop, 30), "Failed to create loop for cleanup test");
    loop.running = true ;

    /* The watchers and their callback data are registered in the loop and used
     * by sse_run() below, so they must outlive each iteration: keep them in
     * frame-scoped arrays, not inside the for-body (which would leave the loop
     * referencing stack memory that is out of scope -> use-after-scope). */
    sse_watcher_t io_w[5], timer_w[5], eventfd_w[5];
    struct test_callback_data io_data[5], timer_data[5], eventfd_data[5];
    int io_wr[5];

    for (int i = 0; i < 5; i++) {
        test_callback_init(&io_data[i]);
        test_callback_init(&timer_data[i]);
        test_callback_init(&eventfd_data[i]);
        io_wr[i] = -1;

        int pipefd[2];
        if (pipe(pipefd) == 0) {
            TEST_ASSERT(sse_start_io(&loop, &io_w[i], test_mixed_callback, &io_data[i], pipefd[0], SSE_READ, i + 1),
                        "Failed to start io watcher");
            io_wr[i] = pipefd[1];
        }

        TEST_ASSERT(sse_start_timer(&loop, &timer_w[i], test_mixed_callback, &timer_data[i], 30 + i * 5, 0, i + 10),
                    "Failed to start timer watcher");
        TEST_ASSERT(sse_start_eventfd(&loop, &eventfd_w[i], test_mixed_callback, &eventfd_data[i], i + 20),
                    "Failed to start eventfd watcher");
    }

    /* All 15 must be active before the loop is driven. */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(sse_watcher_active(&io_w[i]), "io watcher should be active");
        TEST_ASSERT(sse_watcher_active(&timer_w[i]), "timer watcher should be active");
        TEST_ASSERT(sse_watcher_active(&eventfd_w[i]), "eventfd watcher should be active");
    }

    /* Trigger every io and eventfd watcher; the one-shot timers fire on their own. */
    for (int i = 0; i < 5; i++) {
        if (io_wr[i] >= 0)
            TEST_ASSERT(write(io_wr[i], "x", 1) == 1, "Failed to write io trigger");
        TEST_ASSERT(sse_write_eventfd(&eventfd_w[i]), "Failed to write eventfd");
    }

    for (int i = 0; i < 40; i++) {
        if (!sse_run(&loop, 30))
            break;
    }

    /* Every watcher must have been dispatched, not merely registered. */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(io_data[i].callback_called, "each io watcher must fire");
        TEST_ASSERT(timer_data[i].callback_called, "each timer watcher must fire");
        TEST_ASSERT(eventfd_data[i].callback_called, "each eventfd watcher must fire");
    }

    sse_free(&loop);
    for (int i = 0; i < 5; i++)
        if (io_wr[i] >= 0)
            close(io_wr[i]);

    return true;
}

TEST_SUITE_BEGIN("SSE All Watchers Tests - Comprehensive Mixed-Type Testing")
   RUN_TEST(test_all_watchers_comprehensive);
   RUN_TEST(test_mixed_watcher_lifecycle);
   RUN_TEST(test_mixed_watcher_cleanup);
TEST_SUITE_END()