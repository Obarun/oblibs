/*
 * sse.c
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/sse.h>

static int io_count = 0;

static void io_callback(sse_watcher_t *w, void *cbdata, int event)
{
    (void *)cbdata; // silent unused param
    fprintf(stderr,"IO callback: fd=%d, event=0x%x, active=%d watcher=%p\n", w->fd, event, w->active, w);
    assert(w->type == SSE_TYPE_IO);
    assert(event & SSE_READ);
    assert(event & SSE_HUP); // pipe was closed
    assert(!w->active); // pipe was closed, sse_dispatch_io stop the watcher

    char buf[256];
    ssize_t n = read(w->fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        fprintf(stderr,"Read from fd %d: %s", w->fd, buf);
        io_count++;
    } else if (n == 0) {
        fprintf(stderr,"EOF on fd %d, stopping watcher\n", w->fd);
        sse_stop_io(w);
    } else {
        fprintf(stderr,"Read error on fd %d: %s\n", w->fd, strerror(errno));
        sse_stop_io(w);
    }
}

int main(void)
{
    sse_epoll_t epoll;
    sse_watcher_t io_watcher; // Initialize to zero
    int pipefd[2];

    /* Initialize epoll instance */
    fprintf(stderr,"Initializing epoll instance\n");
    assert(sse_new(&epoll, 10));
    assert(epoll.fd >= 0);
    assert(epoll.maxevents == 10);
    assert(epoll.pending != NULL);
    assert(epoll.file != NULL);
    assert(epoll.running);

    /* Create a pipe for controlled input */
    fprintf(stderr,"Creating pipe for input\n");
    assert(pipe(pipefd) == 0);
    const char *test_input = "test\n";

    /* Test 1: Start a new I/O watcher on pipe read end */
    fprintf(stderr,"Test 1: Setting up new I/O watcher for pipe\n");
    assert(sse_start_io(&epoll, &io_watcher, io_callback, NULL, pipefd[0], SSE_READ, 1));
    assert(io_watcher.type == SSE_TYPE_IO);
    assert(io_watcher.fd == pipefd[0]);
    assert(io_watcher.events == SSE_READ);
    assert(io_watcher.active);
    assert(io_watcher.p == &epoll);
    fprintf(stderr,"After sse_start_io, epoll.watchers=%p\n", epoll.watchers);

    assert(write(pipefd[1], test_input, strlen(test_input)) == (ssize_t)strlen(test_input));
    close(pipefd[1]); // Close write end to signal EOF after input

    /* Run event loop */
    fprintf(stderr,"Running event loop\n");
    epoll.running = true;
    int max_iterations = 3;
    fprintf(stderr, "io_watcher->active=%i\n",io_watcher.active) ;
    while (epoll.running && max_iterations--) {
        fprintf(stderr,"Iteration %d, epoll.watchers=%p\n", 3 - max_iterations, epoll.watchers);
        if (!sse_run(&epoll, 1000)) {
            fprintf(stderr,"Error in sse_run: %s\n", strerror(io_watcher.api_errno ? io_watcher.api_errno : errno));
            close(pipefd[0]);
            sse_free(&epoll);
            return 1; // Fail test on error
        }
        if (io_count >= 1) {
            fprintf(stderr,"Received I/O event, stopping loop\n");
            epoll.running = false;
        }
    }

    /* Verify event counts */
    fprintf(stderr,"Verifying event counts: io_count=%d\n", io_count);
    assert(io_count == 1); // Expect exactly one read event from pipe

    /* Clean up */
    fprintf(stderr,"Cleaning up, epoll.watchers=%p\n", epoll.watchers);
    close(pipefd[1]); // Close read end of pipe
    sse_free(&epoll);
    assert(epoll.fd == -1);
    assert(epoll.pending == NULL);
    assert(epoll.watchers == NULL);
    assert(epoll.file == NULL);

    fprintf(stderr,"All tests passed successfully\n");
    return 0;
}