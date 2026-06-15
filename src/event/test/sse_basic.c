/*
 * sse_basic.c
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/inotify.h>

#include <oblibs/sse.h>

// --- Helper Macros ---

#define BEGIN_TEST(name) \
    do { \
        printf("==> Running test: %s\n", #name); \
    } while (0)

#define UNUSED(x) (void)(x)

// --- Dummy Callback ---

void dummy_cb(sse_watcher_t *w, void *data, int e) {
    UNUSED(w); UNUSED(data); UNUSED(e);
}

// --- Test: Hash Ops (via public API) ---

void test_sse_hash_ops(void) {
    BEGIN_TEST(test_sse_hash_ops);

    sse_epoll_t loop = {0};
    sse_watcher_t w1, w2;

    assert(sse_init(&loop, 16) == 1);

    assert(sse_watcher_init(&loop, &w1, SSE_TYPE_IO, dummy_cb, NULL, 1, SSE_READ, 0) == 1);
    assert(sse_watcher_init(&loop, &w2, SSE_TYPE_IO, dummy_cb, NULL, 2, SSE_WRITE, 0) == 1);

    assert(sse_watcher_add(&w1) == 1);
    assert(sse_watcher_add(&w2) == 1);

    sse_watcher_del(&w1);
    sse_watcher_del(&w2);

    sse_free(&loop);
}

// --- Test: Watcher Ops ---

void test_sse_watcher_ops(void) {
    BEGIN_TEST(test_sse_watcher_ops);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    int pipefd[2];
    assert(pipe(pipefd) == 0);
    close(pipefd[1]); // We only read

    assert(sse_new(&loop, 16) == 1);

    assert(sse_watcher_init(&loop, &w, SSE_TYPE_IO, dummy_cb, NULL, pipefd[0], SSE_READ, 0) == 1);
    assert(sse_watcher_add(&w) == 1);
    assert(sse_watcher_active(&w) == true);

    assert(sse_watcher_modify(&w) == 1);
    assert(sse_watcher_restart(&w) == 1);
    assert(sse_watcher_stop(&w) == 1);
    assert(sse_watcher_active(&w) == false);

    sse_watcher_free(&w);
    sse_free(&loop);

    close(pipefd[0]); // Safe to close after free
}

// --- Test: IO Watcher ---

void test_sse_io(void) {
    BEGIN_TEST(test_sse_io);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    assert(sse_new(&loop, 16) == 1);

    int pipefd[2];
    assert(pipe(pipefd) == 0);

    assert(sse_start_io(&loop, &w, dummy_cb, NULL, pipefd[0], SSE_READ, 0) == 1);
    assert(sse_modify_io(&w, pipefd[0], SSE_READ | SSE_WRITE) == 1);
    assert(sse_restart_io(&w) == 1);

    sse_stop_io(&w);
    sse_free_io(&w);

    close(pipefd[0]);
    close(pipefd[1]);
    sse_free(&loop);
}

// --- Test: Signal Watcher ---

void test_sse_signal(void) {
    BEGIN_TEST(test_sse_signal);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_signal(&loop, &w, dummy_cb, NULL, 0) == 1);
    assert(sse_attach_signal(&w, SIGTERM) == 1);
    assert(sse_attach_signal(&w, SIGHUP) == 1);

    assert(sse_restart_signal(&w) == 1);
    assert(sse_stop_signal(&w) == 1);

    sse_free_signal(&w);
    sse_free(&loop);
}

// --- Test: Timer Watcher ---

void test_sse_timer(void) {
    BEGIN_TEST(test_sse_timer);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_timer(&loop, &w, dummy_cb, NULL, 100, 0, 0) == 1);
    assert(sse_modify_timer(&w, 200, 0) == 1);

    assert(sse_restart_timer(&w) == 1);
    assert(sse_stop_timer(&w) == 1);

    sse_free_timer(&w);
    sse_free(&loop);
}

// --- Test: Schedule Watcher ---

void test_sse_schedule(void) {
    BEGIN_TEST(test_sse_schedule);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    cron_t c = CRON_EXPR_ZERO;
    assert(parse_cron("* * * * ?", &c, "UTC") == 1);

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_schedule(&loop, &w, dummy_cb, NULL, &c, 0) == 1);

    time_t now = time(NULL);
    time_t fire = sse_getfire_schedule(&w);
    assert(fire > now);

    sse_stop_schedule(&w);
    sse_free_schedule(&w);
    sse_free(&loop);
}

// --- Test: Child Watcher ---

void test_sse_child(void) {
    BEGIN_TEST(test_sse_child);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    pid_t pid = fork();
    if (pid == 0) {
        usleep(10000);
        exit(0);
    }

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_child(&loop, &w, dummy_cb, NULL, pid, 0, false) == 1);

    assert(sse_restart_child(&w) == 1);
    assert(sse_stop_child(&w) == 1);

    sse_free_child(&w);
    sse_free(&loop);

    int status;
    waitpid(pid, &status, 0);
}

// --- Test: EventFD Watcher ---

void test_sse_eventfd(void) {
    BEGIN_TEST(test_sse_eventfd);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_eventfd(&loop, &w, dummy_cb, NULL, 0) == 1);

    assert(sse_write_eventfd(&w) == 1);
    assert(sse_read_eventfd(&w) == 1);

    assert(sse_restart_eventfd(&w) == 1);
    assert(sse_stop_eventfd(&w) == 1);

    sse_free_eventfd(&w);
    sse_free(&loop);
}

// --- Test: Inotify Watcher ---

void test_sse_inotify(void) {
    BEGIN_TEST(test_sse_inotify);

    sse_epoll_t loop = {0};
    sse_watcher_t w;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_inotify(&loop, &w, dummy_cb, NULL, 0) == 1);

    FILE *f = fopen("/tmp/sse_test_file", "w");
    assert(f != NULL);
    fclose(f);

    assert(sse_attach_inotify(&w, "/tmp/sse_test_file", IN_MODIFY) == 1);

    assert(sse_restart_inotify(&w) == 1);
    assert(sse_stop_inotify(&w) == 1);

    sse_free_inotify(&w);
    sse_free(&loop);

    file_tryunlink("/tmp/sse_test_file");
}

// --- Test: Loop Operations ---

void test_sse_loop_ops(void) {
    BEGIN_TEST(test_sse_loop_ops);

    sse_epoll_t loop = {0};

    assert(sse_new(&loop, 16) == 1);

    assert(sse_prepare(&loop) == 1);
    assert(sse_wait(&loop, 1) >= 0);
    sse_dispatch(&loop);
    assert(sse_run(&loop, 1) == 1);

    sse_free(&loop);
}

// --- Main ---

int main(void) {
    printf("=== Running SSE Unit Tests ===\n");

    test_sse_hash_ops();
    test_sse_watcher_ops();
    test_sse_io();
    test_sse_signal();
    test_sse_timer();
    test_sse_schedule();
    test_sse_child();
    test_sse_eventfd();
    test_sse_inotify();
    test_sse_loop_ops();

    printf("=== All SSE unit tests passed ===\n");
    return 0;
}