/*
 * defer.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/*
 * Free-safety of sse_dispatch:
 *   1. self-free  : a callback frees its own watcher's container -> the reorder
 *      of revents (reset before the callback) makes the post-callback loop safe.
 *   2. sibling-free: a callback frees ANOTHER watcher still pending in the same
 *      cycle -> must be deferred via sse_defer, else FOREACH_QUEUE dereferences
 *      freed memory further down the loop.
 *   3. sse_stream sibling: closing a sibling stream from a stream callback must
 *      not double-free nor UAF (deferred free + closed-guard in the callback).
 *
 * Run under AddressSanitizer (-Db_sanitize=address) to actually prove the
 * absence of use-after-free / double-free; without ASan it only checks that the
 * loop does not crash and that deferred actions run.
 */

#include <sys/socket.h>
#include <unistd.h>

#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include "framework.h"

/* ---- Case 1: self-free ---- */

typedef struct { sse_watcher_t w ; } box_t ;

static int g_self_freed = 0 ;

static void self_cb(sse_watcher_t *w, void *cbdata, int event)
{
    (void)w ; (void)event ;
    box_t *c = (box_t *)cbdata ;
    sse_free_io(&c->w) ;   /* removes from epoll/hash, does NOT free memory */
    g_self_freed = 1 ;
    free(c) ;              /* frees the memory holding c->w */
}

static bool test_self_free(void)
{
    sse_epoll_t loop ;
    int p[2] ;

    TEST_ASSERT(sse_new(&loop, 10), "create loop") ;
    loop.running = true ;
    TEST_ASSERT(pipe(p) == 0, "pipe") ;

    box_t *c = malloc(sizeof(*c)) ;
    TEST_ASSERT_NOT_NULL(c, "alloc container") ;
    TEST_ASSERT(sse_start_io(&loop, &c->w, self_cb, c, p[0], SSE_READ, 1), "start") ;

    g_self_freed = 0 ;
    TEST_ASSERT(write(p[1], "x", 1) == 1, "trigger") ;
    TEST_ASSERT(sse_run(&loop, 100), "run") ;
    TEST_ASSERT(g_self_freed == 1, "self callback ran and freed its container") ;

    sse_free(&loop) ;
    close(p[1]) ;          /* p[0] was closed by sse_free_io */
    return true ;
}

/* ---- Case 2: sibling-free via sse_defer ---- */

static sse_epoll_t *g_loop = NULL ;
static box_t *g_victim = NULL ;
static int g_victim_freed = 0 ;

static void free_and_mark(void *arg)
{
    g_victim_freed = 1 ;
    free(arg) ;
}

static void killer_cb(sse_watcher_t *w, void *cbdata, int event)
{
    (void)w ; (void)cbdata ; (void)event ;
    if (g_victim) {
        sse_free_io(&g_victim->w) ;                 /* unhook the sibling */
        sse_defer(g_loop, free_and_mark, g_victim) ; /* free it AFTER dispatch */
        g_victim = NULL ;
    }
}

static void victim_cb(sse_watcher_t *w, void *cbdata, int event)
{
    (void)w ; (void)cbdata ; (void)event ; /* harmless: only proves no UAF on touch */
}

static bool test_sibling_free_defer(void)
{
    sse_epoll_t loop ;
    int pa[2], pb[2] ;
    sse_watcher_t wa ;

    TEST_ASSERT(sse_new(&loop, 10), "create loop") ;
    loop.running = true ;
    g_loop = &loop ;
    TEST_ASSERT(pipe(pa) == 0, "pipe a") ;
    TEST_ASSERT(pipe(pb) == 0, "pipe b") ;

    box_t *b = malloc(sizeof(*b)) ;
    TEST_ASSERT_NOT_NULL(b, "alloc victim") ;

    /* A has higher priority (lower number) -> dispatched before B in the same cycle. */
    TEST_ASSERT(sse_start_io(&loop, &wa, killer_cb, NULL, pa[0], SSE_READ, 1), "start A") ;
    TEST_ASSERT(sse_start_io(&loop, &b->w, victim_cb, b, pb[0], SSE_READ, 2), "start B") ;

    g_victim = b ;
    g_victim_freed = 0 ;
    TEST_ASSERT(write(pa[1], "x", 1) == 1, "trigger A") ;
    TEST_ASSERT(write(pb[1], "y", 1) == 1, "trigger B") ;

    TEST_ASSERT(sse_run(&loop, 100), "run") ;
    TEST_ASSERT(g_victim_freed == 1, "victim freed by deferred drain, after dispatch") ;

    sse_free(&loop) ;      /* frees A's watcher; B already unhooked */
    close(pa[1]) ; close(pb[1]) ; /* read ends closed by sse */
    return true ;
}

/* ---- Case 3: sse_stream sibling close ---- */

static sse_stream_t *g_stream_b = NULL ;
static int g_a_read = 0 ;

static void a_read_cb(sse_stream_t *s, void *data)
{
    (void)s ; (void)data ;
    g_a_read = 1 ;
    if (g_stream_b) {
        sse_stream_close(g_stream_b) ; /* close the sibling from A's callback */
        g_stream_b = NULL ;
    }
}

static void noop_read_cb(sse_stream_t *s, void *data) { (void)s ; (void)data ; }
static void noop_cb(sse_stream_t *s, void *data) { (void)s ; (void)data ; }
static void noop_error_cb(sse_stream_t *s, int e, void *data) { (void)s ; (void)e ; (void)data ; }

static bool test_stream_sibling(void)
{
    sse_epoll_t loop ;
    int sa[2], sb[2] ;

    TEST_ASSERT(sse_new(&loop, 10), "create loop") ;
    loop.running = true ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sa) == 0, "socketpair a") ;
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sb) == 0, "socketpair b") ;

    sse_stream_t *A = sse_stream_new(sa[0], a_read_cb, noop_cb, noop_cb, noop_error_cb, NULL) ;
    sse_stream_t *B = sse_stream_new(sb[0], noop_read_cb, noop_cb, noop_cb, noop_error_cb, NULL) ;
    TEST_ASSERT_NOT_NULL(A, "stream A") ;
    TEST_ASSERT_NOT_NULL(B, "stream B") ;

    /* A higher priority -> dispatched before B in the same cycle. */
    TEST_ASSERT(sse_stream_attach(A, &loop, 1), "attach A") ;
    TEST_ASSERT(sse_stream_attach(B, &loop, 2), "attach B") ;

    g_stream_b = B ;
    g_a_read = 0 ;
    TEST_ASSERT(write(sa[1], "x", 1) == 1, "trigger A") ;
    TEST_ASSERT(write(sb[1], "y", 1) == 1, "trigger B") ;

    TEST_ASSERT(sse_run(&loop, 100), "run") ;
    TEST_ASSERT(g_a_read == 1, "A read callback ran") ;
    /* B was closed during A's callback: deferred free + closed-guard => no UAF,
     * no double-free (proven under ASan). */

    sse_stream_close(A) ;  /* deferred; drained by sse_free below */
    sse_free(&loop) ;
    close(sa[1]) ; close(sb[1]) ; /* stream-owned ends closed by cleanup */
    return true ;
}

TEST_SUITE_BEGIN("SSE dispatch free-safety (self-free, sibling-free, stream sibling)")
    RUN_TEST(test_self_free) ;
    RUN_TEST(test_sibling_free_defer) ;
    RUN_TEST(test_stream_sibling) ;
TEST_SUITE_END()
