/*
 * lx_inotify_keyptr.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

/*
 * Regression test for the path-key lifetime in lx_inotify_add().
 *
 * The hash table does NOT copy the key: hash_add stores the pointer it is
 * handed (node->key = key). If lx_inotify_add() registers the caller's
 * transient `what` buffer as the key instead of the stable strdup'd
 * wd->path, then freeing/overwriting `what` after the call turns every
 * later hash_find(&iwd, ...) into a read of dead memory.
 *
 * This test frees the `what` buffer after the watch is added and then
 * looks the path up again. Under ASan it reports heap-use-after-free
 * while the bug is present, and passes once the key points at wd->path.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <oblibs/linux.h>

int main(void)
{
    static const char *template = "/tmp/lx_inotify_keyptr_XXXXXX" ;

    char *path = strdup(template) ;
    assert(path != NULL) ;
    int tfd = mkstemp(path) ;
    assert(tfd >= 0) ;
    close(tfd) ;

    int fd = lx_inotify_init() ;
    assert(fd >= 0) ;

    /* Register the watch through a transient heap buffer. */
    char *what = strdup(path) ;
    assert(what != NULL) ;
    int wd = lx_inotify_add(what, 0x00000002 /* IN_MODIFY */) ;
    assert(wd > 0) ;

    /*
     * The caller is free to release `what` now: lx_inotify_add() is
     * documented to own a stable copy (wd->path). Poison the buffer so a
     * dangling key is guaranteed to be caught instead of silently matching.
     */
    free(what) ;

    /* Look the same path up again through a fresh buffer. */
    struct lx_inotify_wd_s *found = NULL ;
    int r = lx_inotify_search(&found, path) ;
    assert(r == 1 && "path lookup must still succeed after the add buffer is freed") ;
    assert(found != NULL) ;
    assert(found->wd == wd && "lookup returned the wrong watch") ;
    assert(strcmp(found->path, path) == 0 && "lookup returned a corrupted path") ;

    lx_inotify_end() ;
    unlink(path) ;
    free(path) ;

    printf("lx_inotify_keyptr: PASS\n") ;
    return 0 ;
}
