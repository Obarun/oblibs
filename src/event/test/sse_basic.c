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
 * except according to the terms contained in the LICENSE file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/inotify.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>
#include <oblibs/files.h>

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

// --- Test: IO Watcher (lifecycle) ---

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

// --- Test: IO Watcher (real event triggered + read effect) ---

struct io_trigger_s {
    int calls ;
    int last_event ;
    char buf[64] ;
    ssize_t nread ;
} ;

static void io_trigger_cb(sse_watcher_t *w, void *data, int e) {
    struct io_trigger_s *s = data ;
    s->calls++ ;
    s->last_event = e ;
    if (e & SSE_READ)
        s->nread = read(w->fd, s->buf, sizeof(s->buf) - 1) ;
}

void test_sse_io_trigger(void) {
    BEGIN_TEST(test_sse_io_trigger);

    sse_epoll_t loop = {0};
    sse_watcher_t w;
    struct io_trigger_s s = {0};

    int pipefd[2];
    assert(pipe(pipefd) == 0);

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_io(&loop, &w, io_trigger_cb, &s, pipefd[0], SSE_READ, 0) == 1);

    const char *msg = "ping" ;
    assert(write(pipefd[1], msg, 4) == 4) ;

    // Single iteration must deliver the read event and the callback must run.
    assert(sse_run(&loop, 200) == 1) ;

    /* Callback fired with SSE_READ and read the exact bytes written.
     * Mutation: emptying sse_dispatch_io leaves the dispatch path intact for
     * IO (it only stops on SSE_ERROR), so this asserts the full wait+dispatch
     * wiring rather than dispatch_io itself. */
    assert(s.calls == 1) ;
    assert(s.last_event & SSE_READ) ;
    assert(s.nread == 4) ;
    s.buf[s.nread] = '\0' ;
    assert(strcmp(s.buf, "ping") == 0) ;

    sse_free(&loop);
    close(pipefd[0]);
    close(pipefd[1]);
}

// --- Test: Signal Watcher (lifecycle) ---

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

// --- Test: Signal Watcher (real signal delivered, coalesced into sdata) ---

static int signal_calls = 0 ;

static void signal_trigger_cb(sse_watcher_t *w, void *data, int e) {
    UNUSED(w); UNUSED(data); UNUSED(e);
    signal_calls++ ;
}

void test_sse_signal_trigger(void) {
    BEGIN_TEST(test_sse_signal_trigger);

    sse_epoll_t loop = {0};
    sse_watcher_t w;
    signal_calls = 0 ;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_signal(&loop, &w, signal_trigger_cb, NULL, 0) == 1);
    // sse_attach_signal blocks the signal process-wide so signalfd receives it.
    assert(sse_attach_signal(&w, SIGUSR1) == 1);

    assert(raise(SIGUSR1) == 0) ;

    int iter = 0 ;
    while (iter < 5 && !signal_calls) {
        assert(sse_run(&loop, 200) == 1) ;
        iter++ ;
    }

    assert(signal_calls == 1) ;

    /* Real effect: sse_dispatch_signal drained the signalfd and stored the
     * last siginfo in sse_signal_t.si. Mutation: emptying sse_dispatch_signal
     * leaves ssi_signo at 0 (sdata is zero-initialized at start). */
    sse_signal_t *sig = (sse_signal_t *)w.sdata ;
    assert(sig != NULL) ;
    assert(sig->si.ssi_signo == (uint32_t)SIGUSR1) ;

    sse_free(&loop);
}

// --- Test: Timer Watcher (lifecycle) ---

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

// --- Test: Timer Watcher (real expiry + drain proof) ---

static int timer_calls = 0 ;

static void timer_trigger_cb(sse_watcher_t *w, void *data, int e) {
    UNUSED(w); UNUSED(data); UNUSED(e);
    timer_calls++ ;
}

void test_sse_timer_trigger(void) {
    BEGIN_TEST(test_sse_timer_trigger);

    sse_epoll_t loop = {0};
    sse_watcher_t w;
    timer_calls = 0 ;

    assert(sse_new(&loop, 16) == 1);
    // One-shot 50 ms timer (no period).
    assert(sse_start_timer(&loop, &w, timer_trigger_cb, NULL, 50, 0, 0) == 1);

    int iter = 0 ;
    while (iter < 10 && !timer_calls) {
        assert(sse_run(&loop, 200) == 1) ;
        iter++ ;
    }

    // The one-shot timer expired exactly once and the callback ran.
    assert(timer_calls == 1) ;

    /* Drain proof: sse_dispatch_timer read() the expiration off the timerfd.
     * A further run with no new expiry must NOT re-fire. Mutation: emptying
     * sse_dispatch_timer leaves the timerfd readable, so this extra run would
     * re-enter the callback and bump timer_calls to 2. */
    assert(sse_run(&loop, 100) == 1) ;
    assert(timer_calls == 1) ;

    sse_stop_timer(&w) ;
    sse_free(&loop);
}

// --- Test: Schedule Watcher (lifecycle + fire time) ---

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

// --- Test: Child Watcher (lifecycle) ---

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

// --- Test: Child Watcher (real exit reaped, status captured) ---

static int child_calls = 0 ;

static void child_trigger_cb(sse_watcher_t *w, void *data, int e) {
    UNUSED(w); UNUSED(data); UNUSED(e);
    child_calls++ ;
}

void test_sse_child_trigger(void) {
    BEGIN_TEST(test_sse_child_trigger);

    sse_epoll_t loop = {0};
    sse_watcher_t w;
    child_calls = 0 ;

    pid_t pid = fork();
    assert(pid >= 0) ;
    if (pid == 0) {
        usleep(20000) ;
        _exit(42) ;
    }

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_child(&loop, &w, child_trigger_cb, NULL, pid, 0, false) == 1);

    int iter = 0 ;
    while (iter < 20 && !child_calls) {
        assert(sse_run(&loop, 200) == 1) ;
        iter++ ;
    }

    // The pidfd became readable on child exit and the callback ran.
    assert(child_calls == 1) ;

    /* Real effect: sse_dispatch_child called waitpid(WNOHANG) and stored the
     * wait status in sse_child_t.status. Mutation: emptying sse_dispatch_child
     * leaves status at 0 (so WIFEXITED/WEXITSTATUS would not see 42), and the
     * zombie stays unreaped. */
    sse_child_t *ch = (sse_child_t *)w.sdata ;
    assert(ch != NULL) ;
    assert(WIFEXITED(ch->status)) ;
    assert(WEXITSTATUS(ch->status) == 42) ;

    sse_free(&loop);
    // Child already reaped by dispatch; a second wait must report ECHILD.
    int status ;
    assert(waitpid(pid, &status, 0) == -1 && errno == ECHILD) ;
}

// --- Test: EventFD Watcher (lifecycle) ---

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

// --- Test: EventFD Watcher (real signaling + drain proof) ---

static int eventfd_calls = 0 ;

static void eventfd_trigger_cb(sse_watcher_t *w, void *data, int e) {
    UNUSED(w); UNUSED(data); UNUSED(e);
    eventfd_calls++ ;
}

void test_sse_eventfd_trigger(void) {
    BEGIN_TEST(test_sse_eventfd_trigger);

    sse_epoll_t loop = {0};
    sse_watcher_t w;
    eventfd_calls = 0 ;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_eventfd(&loop, &w, eventfd_trigger_cb, NULL, 0) == 1);

    // Raise the eventfd counter; the watcher fd becomes readable.
    assert(sse_write_eventfd(&w) == 1);

    int iter = 0 ;
    while (iter < 5 && !eventfd_calls) {
        assert(sse_run(&loop, 200) == 1) ;
        iter++ ;
    }

    assert(eventfd_calls == 1) ;

    /* Drain proof: sse_dispatch_eventfd read() the counter, so the fd is no
     * longer readable. A further run with no new write must NOT re-fire.
     * Mutation: emptying sse_dispatch_eventfd leaves the counter set (level
     * triggered), so this extra run would bump eventfd_calls to 2. */
    assert(sse_run(&loop, 100) == 1) ;
    assert(eventfd_calls == 1) ;

    sse_free(&loop);
}

// --- Test: Inotify Watcher (lifecycle) ---

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

// --- Test: Inotify Watcher (real file modification + drain proof) ---

static int inotify_calls = 0 ;

static void inotify_trigger_cb(sse_watcher_t *w, void *data, int e) {
    UNUSED(w); UNUSED(data); UNUSED(e);
    inotify_calls++ ;
}

void test_sse_inotify_trigger(void) {
    BEGIN_TEST(test_sse_inotify_trigger);

    const char *path = "/tmp/sse_test_inotify_trigger" ;
    sse_epoll_t loop = {0};
    sse_watcher_t w;
    inotify_calls = 0 ;

    file_tryunlink(path) ;
    FILE *f = fopen(path, "w") ;
    assert(f != NULL) ;
    fclose(f) ;

    assert(sse_new(&loop, 16) == 1);
    assert(sse_start_inotify(&loop, &w, inotify_trigger_cb, NULL, 0) == 1);
    assert(sse_attach_inotify(&w, path, IN_MODIFY) == 1);

    // Modify the watched file: this must produce an IN_MODIFY inotify event.
    int fd = open(path, O_WRONLY | O_APPEND) ;
    assert(fd >= 0) ;
    assert(write(fd, "x", 1) == 1) ;
    close(fd) ;

    int iter = 0 ;
    while (iter < 5 && !inotify_calls) {
        assert(sse_run(&loop, 200) == 1) ;
        iter++ ;
    }

    assert(inotify_calls == 1) ;

    /* Drain proof: sse_dispatch_inotify read() the event buffer, so the fd is
     * no longer readable. A further run with no new change must NOT re-fire.
     * Mutation: emptying sse_dispatch_inotify leaves the inotify fd readable,
     * so this extra run would bump inotify_calls to 2. */
    assert(sse_run(&loop, 100) == 1) ;
    assert(inotify_calls == 1) ;

    sse_free(&loop);
    file_tryunlink(path) ;
}

// --- Main ---

int main(void) {

    VERBOSITY = LOG_LEVEL_WARN;
    PROG = "sse_basic";
    printf("=== Running SSE Unit Tests ===\n");

    test_sse_hash_ops();
    test_sse_watcher_ops();
    test_sse_io();
    test_sse_io_trigger();
    test_sse_signal();
    test_sse_signal_trigger();
    test_sse_timer();
    test_sse_timer_trigger();
    test_sse_schedule();
    test_sse_child();
    test_sse_child_trigger();
    test_sse_eventfd();
    test_sse_eventfd_trigger();
    test_sse_inotify();
    test_sse_inotify_trigger();

    printf("=== All SSE unit tests passed ===\n");
    return 0;
}
