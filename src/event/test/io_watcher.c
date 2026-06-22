/*
 * io_watcher.c
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

#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <oblibs/sse.h>
#include <oblibs/files.h>
#include "framework.h"

/* Test callback for I/O events */
static void test_io_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store watcher pointer to identify which watcher triggered */
    data->user_data = w;
}

/* Main I/O watcher test - single loop handling multiple I/O types */
static bool test_io_watchers_comprehensive(void) {
    sse_epoll_t loop;

    /* Multiple watchers for different I/O types */
    sse_watcher_t pipe_watcher, socket_watcher, fifo_watcher;
    struct test_callback_data pipe_data, socket_data, fifo_data, file_data;

    /* File descriptors */
    int pipefd[2], sockfd[2], fifo_read, fifo_write, file_fd;
    const char *fifo_path = "/tmp/test_sse_fifo";
    const char *test_file = "/tmp/test_sse_file";

    /* Test data */
    char pipe_msg[] = "pipe data";
    char socket_msg[] = "socket data";
    char fifo_msg[] = "fifo data";
    char file_content[] = "file content\n";

    /* Initialize callback data */
    test_callback_init(&pipe_data);
    test_callback_init(&socket_data);
    test_callback_init(&fifo_data);
    test_callback_init(&file_data);

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true ;
    /* === Setup Pipe === */
    TEST_ASSERT(pipe(pipefd) == 0, "Failed to create pipe");
    TEST_ASSERT(sse_start_io(&loop, &pipe_watcher, test_io_callback, &pipe_data,
                            pipefd[0], SSE_READ, 1),
            "Failed to start pipe watcher");

    /* === Setup Socket === */
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) == 0,
            "Failed to create socket pair");
    TEST_ASSERT(sse_start_io(&loop, &socket_watcher, test_io_callback, &socket_data,
                            sockfd[0], SSE_READ, 2),
            "Failed to start socket watcher");

    /* === Setup FIFO === */
    file_tryunlink(fifo_path); /* Clean up any existing FIFO */
    TEST_ASSERT(mkfifo(fifo_path, 0666) == 0, "Failed to create FIFO");
    fifo_read = open(fifo_path, O_RDONLY | O_NONBLOCK);
    TEST_ASSERT(fifo_read >= 0, "Failed to open FIFO for reading");
    TEST_ASSERT(sse_start_io(&loop, &fifo_watcher, test_io_callback, &fifo_data,
                            fifo_read, SSE_READ, 3),
            "Failed to start FIFO watcher");

    /* === Setup File === */
    file_fd = open(test_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    TEST_ASSERT(file_fd >= 0, "Failed to create test file");
    TEST_ASSERT(write(file_fd, file_content, strlen(file_content)) > 0,
            "Failed to write test data to file");
    close(file_fd);

    /* === Verify all watchers are active === */
    TEST_ASSERT(sse_watcher_active(&pipe_watcher), "Pipe watcher should be active");
    TEST_ASSERT(sse_watcher_active(&socket_watcher), "Socket watcher should be active");
    TEST_ASSERT(sse_watcher_active(&fifo_watcher), "FIFO watcher should be active");
    //TEST_ASSERT(sse_watcher_active(&file_watcher), "File watcher should be active");

    /* === Trigger events === */

    /* Trigger pipe event */
    TEST_ASSERT(write(pipefd[1], pipe_msg, sizeof(pipe_msg)) > 0,
            "Failed to write to pipe");

    /* Trigger socket event */
    TEST_ASSERT(send(sockfd[1], socket_msg, sizeof(socket_msg), 0) > 0,
            "Failed to send socket data");

    /* Trigger FIFO event */
    fifo_write = open(fifo_path, O_WRONLY);
    TEST_ASSERT(fifo_write >= 0, "Failed to open FIFO for writing");
    TEST_ASSERT(write(fifo_write, fifo_msg, sizeof(fifo_msg)) > 0,
            "Failed to write to FIFO");
    close(fifo_write);

    /* === Process events with single loop === */
    int max_iterations = 10;
    int processed_events = 0;

    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 100)) {
            break;
        }

        /* Count how many different I/O types have been processed */
        processed_events = 0;
        if (pipe_data.callback_called) processed_events++;
        if (socket_data.callback_called) processed_events++;
        if (fifo_data.callback_called) processed_events++;
        //if (file_data.callback_called) processed_events++;

        /* If we've processed most events, we can break early */
        if (processed_events >= 3) break;
    }

    /* === Verify all events were handled === */
    TEST_ASSERT(pipe_data.callback_called, "Pipe callback should have been called");
    TEST_ASSERT(socket_data.callback_called, "Socket callback should have been called");
    TEST_ASSERT(fifo_data.callback_called, "FIFO callback should have been called");
    //TEST_ASSERT(file_data.callback_called, "File callback should have been called");

    /* === Verify event types === */
    TEST_ASSERT(pipe_data.last_event & SSE_READ, "Pipe should receive read event");
    TEST_ASSERT(socket_data.last_event & SSE_READ, "Socket should receive read event");
    TEST_ASSERT(fifo_data.last_event & SSE_READ, "FIFO should receive read event");
    //TEST_ASSERT(file_data.last_event & SSE_READ, "File should receive read event");

    /* === Test priority ordering === */
    /* Lower priority number = higher priority, so file_watcher (priority 4) should be processed last */
    /* This is a rough check - actual timing may vary */

    /* === Test watcher modification: change events on the SAME fd === */
    /* Switch the socket watcher (sockfd[0]) from SSE_READ to SSE_WRITE. A
     * connected stream socket has space in its send buffer, so it is writable
     * and a real run cycle must now deliver an SSE_WRITE event. This proves the
     * epoll EPOLL_CTL_MOD actually re-armed the fd, not just that the struct
     * fields were updated. We first drain the readable data so SSE_READ can no
     * longer be the source of the callback. */
    {
        int fl = fcntl(sockfd[0], F_GETFL, 0);
        fcntl(sockfd[0], F_SETFL, fl | O_NONBLOCK);
        char drain[256];
        while (recv(sockfd[0], drain, sizeof(drain), 0) > 0) ;
        fcntl(sockfd[0], F_SETFL, fl);
    }

    TEST_ASSERT(sse_modify_io(&socket_watcher, sockfd[0], SSE_WRITE),
            "Failed to modify socket watcher to write mode");
    TEST_ASSERT_EQ(sockfd[0], socket_watcher.fd, "Socket watcher fd should be unchanged");
    TEST_ASSERT_EQ(SSE_WRITE, socket_watcher.events, "Socket watcher events should be updated");

    socket_data.callback_called = false;
    socket_data.last_event = 0;

    int got_write = 0;
    for (int i = 0; i < 10 && !got_write; i++) {
        if (!sse_run(&loop, 100)) break;
        if (socket_data.callback_called && (socket_data.last_event & SSE_WRITE))
            got_write = 1;
    }
    TEST_ASSERT(got_write, "Modified socket watcher must fire SSE_WRITE after sse_modify_io");

    /* Put the socket watcher back to SSE_READ for the stop/restart checks. */
    TEST_ASSERT(sse_modify_io(&socket_watcher, sockfd[0], SSE_READ),
            "Failed to restore socket watcher to read mode");

    /*
     * Changing the fd itself (not just the events) is honoured: sse_modify_io()
     * removes the old fd from epoll and adds the new one, rather than a MOD on
     * the never-registered new fd -- which epoll rejects with a silently
     * tolerated ENOENT, leaving the old fd armed and the new one dark.
     */
    {
        sse_epoll_t bloop;
        sse_watcher_t bw;
        struct test_callback_data bdata;
        int bp[2];

        test_callback_init(&bdata);
        TEST_ASSERT(sse_new(&bloop, 10), "Failed to create epoll loop for fd-change check");
        bloop.running = true;
        TEST_ASSERT(pipe(bp) == 0, "Failed to create pipe for fd-change check");

        /* Watch the read end for READ, then switch to the write end for WRITE.
         * A pipe write end is always writable, so SSE_WRITE must now fire on the
         * new fd. */
        TEST_ASSERT(sse_start_io(&bloop, &bw, test_io_callback, &bdata, bp[0], SSE_READ, 1),
                "Failed to start watcher for fd-change check");
        TEST_ASSERT(sse_modify_io(&bw, bp[1], SSE_WRITE),
                "sse_modify_io to a new fd should succeed");

        bool fired = false;
        for (int i = 0; i < 5; i++) {
            if (!sse_run(&bloop, 50)) break;
            if (bdata.callback_called) { fired = true; break; }
        }
        TEST_ASSERT(fired, "new fd must be armed: SSE_WRITE should fire after the fd change");
        TEST_ASSERT(bdata.last_event & SSE_WRITE, "delivered event should be SSE_WRITE on the new fd");

        sse_free(&bloop);
        close(bp[0]);
        close(bp[1]);
    }

    /* === Test watcher stop (observable: no more delivery) === */
    /* The socket read watcher (sockfd[0]) still has unread data from earlier
     * (socket_msg). After stopping it, repeated run cycles must NOT deliver any
     * event for it, proving the epoll EPOLL_CTL_DEL happened. */
    TEST_ASSERT(sse_stop_io(&socket_watcher), "Failed to stop socket watcher");
    TEST_ASSERT(!sse_watcher_active(&socket_watcher), "Socket watcher should be inactive");

    socket_data.callback_called = false;
    socket_data.last_event = 0;

    /* Make sure there is readable data on sockfd[0] so that, were the watcher
     * still armed, it would fire immediately. */
    TEST_ASSERT(send(sockfd[1], "more", 4, 0) > 0, "Failed to send extra socket data");

    for (int i = 0; i < 5; i++) {
        if (!sse_run(&loop, 50)) break;
    }
    TEST_ASSERT(!socket_data.callback_called,
            "Stopped socket watcher must not deliver events");

    /* === Test watcher restart (observable: delivery resumes) === */
    /* Data is still queued on sockfd[0]; after restart a run cycle must deliver
     * an SSE_READ event again. */
    TEST_ASSERT(sse_restart_io(&socket_watcher), "Failed to restart socket watcher");
    TEST_ASSERT(sse_watcher_active(&socket_watcher), "Socket watcher should be active again");

    socket_data.callback_called = false;
    socket_data.last_event = 0;

    int got_read = 0;
    for (int i = 0; i < 10 && !got_read; i++) {
        if (!sse_run(&loop, 100)) break;
        if (socket_data.callback_called && (socket_data.last_event & SSE_READ))
            got_read = 1;
    }
    TEST_ASSERT(got_read, "Restarted socket watcher must deliver SSE_READ again");

    /* === Cleanup === */
    sse_free(&loop);
    close(pipefd[0]);
    close(sockfd[1]);
    file_tryunlink(fifo_path);
    //file_tryunlink(test_file);

    return true;
}

/* Test I/O watcher error handling in a single loop */
static bool test_io_watchers_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;
    int pipefd[2];

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true;
    TEST_ASSERT(pipe(pipefd) == 0, "Failed to create pipe");

    /* Start first watcher successfully */
    TEST_ASSERT(sse_start_io(&loop, &watcher1, test_io_callback, &cb_data1,
                            pipefd[0], SSE_READ, 1),
            "Failed to start first I/O watcher");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_io(NULL, &watcher2, test_io_callback, &cb_data2,
                            pipefd[1], SSE_WRITE, 2),
            "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_io(&loop, NULL, test_io_callback, &cb_data2,
                            pipefd[1], SSE_WRITE, 2),
            "Should fail with NULL watcher");

    TEST_ASSERT(!sse_start_io(&loop, &watcher2, test_io_callback, &cb_data2,
                            -1, SSE_READ, 2),
            "Should fail with invalid fd");

    /* Test that the first watcher still works after error conditions */
    TEST_ASSERT(write(pipefd[1], "test", 4) > 0, "Failed to write test data");
    TEST_ASSERT(sse_run(&loop, 100), "Failed to run event loop");
    TEST_ASSERT(cb_data1.callback_called, "First watcher should still work");

    close(pipefd[1]);
    sse_free(&loop);

    return true;
}

/* Test mixed I/O operations with edge cases */
static bool test_io_watchers_edge_cases(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2, watcher3;
    struct test_callback_data cb_data1, cb_data2, cb_data3;
    int pipefd[2], sockfd[2];

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);
    test_callback_init(&cb_data3);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;
    /* Setup multiple pipes and sockets */
    TEST_ASSERT(pipe(pipefd) == 0, "Failed to create pipe");
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) == 0,
            "Failed to create socket pair");

    /* Start watchers */
    TEST_ASSERT(sse_start_io(&loop, &watcher1, test_io_callback, &cb_data1,
                            pipefd[0], SSE_READ, 1),
            "Failed to start pipe read watcher");

    TEST_ASSERT(sse_start_io(&loop, &watcher2, test_io_callback, &cb_data2,
                            pipefd[1], SSE_WRITE, 2),
            "Failed to start pipe write watcher");

    TEST_ASSERT(sse_start_io(&loop, &watcher3, test_io_callback, &cb_data3,
                            sockfd[0], SSE_READ | SSE_WRITE, 3),
            "Failed to start socket read/write watcher");

    /* Test immediate write availability */
    TEST_ASSERT(sse_run(&loop, 50), "Failed to run event loop");

    /* Write watcher should be immediately ready */
    TEST_ASSERT(cb_data2.callback_called, "Write watcher should be immediately ready");
    TEST_ASSERT(cb_data2.last_event & SSE_WRITE, "Should receive write event");

    /* Test read after write */
    TEST_ASSERT(write(pipefd[1], "data", 4) > 0, "Failed to write to pipe");
    TEST_ASSERT(send(sockfd[1], "msg", 3, 0) > 0, "Failed to send socket data");

    cb_data1.callback_called = false; /* Reset for read test */
    cb_data3.callback_called = false; /* Reset for socket test */

    TEST_ASSERT(sse_run(&loop, 100), "Failed to run event loop for read events");

    TEST_ASSERT(cb_data1.callback_called, "Pipe read should be triggered");
    TEST_ASSERT(cb_data3.callback_called, "Socket read should be triggered");

    sse_free(&loop);
    close(sockfd[1]);

    return true;
}

/* Test that closing the peer surfaces EPOLLHUP/EPOLLERR to the callback */
static bool test_io_watchers_peer_hangup(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher;
    struct test_callback_data cb_data;
    int sockfd[2];

    test_callback_init(&cb_data);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) == 0,
            "Failed to create socket pair");

    /* Watch the read end for read events. */
    TEST_ASSERT(sse_start_io(&loop, &watcher, test_io_callback, &cb_data,
                            sockfd[0], SSE_READ, 1),
            "Failed to start socket watcher");

    /* Close the peer: the watched fd must report a hangup/error condition. */
    close(sockfd[1]);

    int got_hup = 0;
    for (int i = 0; i < 10 && !got_hup; i++) {
        if (!sse_run(&loop, 100)) break;
        if (cb_data.callback_called &&
            (cb_data.last_event & (SSE_HUP | SSE_ERROR | SSE_SHUT)))
            got_hup = 1;
    }

    TEST_ASSERT(cb_data.callback_called,
            "Peer hangup must wake the watcher callback");
    TEST_ASSERT(cb_data.last_event & (SSE_HUP | SSE_ERROR | SSE_SHUT),
            "Peer hangup must surface SSE_HUP/SSE_ERROR/SSE_SHUT to the callback");

    sse_free(&loop);
    close(sockfd[0]);

    return true;
}

TEST_SUITE_BEGIN("SSE I/O Watcher Tests - Single Loop Multiple Types")
    RUN_TEST(test_io_watchers_comprehensive);
    RUN_TEST(test_io_watchers_error_handling);
   RUN_TEST(test_io_watchers_edge_cases);
    RUN_TEST(test_io_watchers_peer_hangup);
TEST_SUITE_END()