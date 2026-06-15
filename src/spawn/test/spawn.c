/*
 * spawn.c
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
 *
 * Behavioural test for spawn_path / spawn_path_merge. Each case spawns a real
 * child and the parent inspects its exit status -- spawn_path itself does not
 * wait, so the test owns the wait(), mirroring the supervision contract.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <oblibs/spawn.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

/* PATH known to contain true/false/sh on every supported system. */
static char const *const baseenv[] = { "PATH=/usr/bin:/bin", 0 } ;

/* Wait for a spawned child and return its exit code, or -1 if signaled. */
static int waitcode (pid_t pid)
{
    int wstat ;
    if (waitpid(pid, &wstat, 0) < 0) { perror("waitpid") ; exit(1) ; }
    return WIFEXITED(wstat) ? WEXITSTATUS(wstat) : -1 ;
}

int main (void)
{
    printf("Running spawn...") ;

    /* spawn_path: absolute path, exit status propagated. */
    {
        char const *argv[] = { "/bin/true", 0 } ;
        pid_t pid = spawn_path("/bin/true", argv, baseenv) ;
        TEST_ASSERT(pid > 0, "spawn /bin/true returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 0, "/bin/true exits 0") ;
    }
    {
        char const *argv[] = { "/bin/false", 0 } ;
        pid_t pid = spawn_path("/bin/false", argv, baseenv) ;
        TEST_ASSERT(pid > 0, "spawn /bin/false returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 1, "/bin/false exits 1") ;
    }

    /* spawn_path: bare name resolved through the caller's PATH. */
    {
        char const *argv[] = { "true", 0 } ;
        setenv("PATH", "/usr/bin:/bin", 1) ;
        pid_t pid = spawn_path("true", argv, baseenv) ;
        TEST_ASSERT(pid > 0, "spawn bare 'true' returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 0, "bare 'true' found via PATH, exits 0") ;
    }

    /* spawn_path: missing file -> 0 + errno (mainstream libc reports execve
     * failure through posix_spawn). */
    {
        char const *argv[] = { "/nonexistent/nope", 0 } ;
        errno = 0 ;
        pid_t pid = spawn_path("/nonexistent/nope", argv, baseenv) ;
        TEST_ASSERT(pid == 0, "missing file returns 0") ;
        TEST_ASSERT(errno == ENOENT, "missing file sets errno = ENOENT") ;
    }

    /* spawn_path: the child must start with an EMPTY signal mask even if the
     * parent blocked a signal (the signalfd supervision case). The child traps
     * SIGTERM then signals itself: with an empty mask the trap fires
     * synchronously (trapped=1, exit 1); if it had inherited SIGTERM blocked,
     * the signal stays pending, the trap never runs before exit (trapped=0,
     * exit 0). */
    {
        sigset_t block, old ;
        sigemptyset(&block) ;
        sigaddset(&block, SIGTERM) ;
        if (sigprocmask(SIG_BLOCK, &block, &old) < 0) { perror("sigprocmask") ; exit(1) ; }

        char const *argv[] = { "/bin/sh", "-c",
            "trapped=0 ; trap 'trapped=1' TERM ; kill -TERM $$ ; exit $trapped", 0 } ;
        pid_t pid = spawn_path("/bin/sh", argv, baseenv) ;
        TEST_ASSERT(pid > 0, "spawn /bin/sh returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 1, "child has SIGTERM unblocked (trap fires)") ;

        if (sigprocmask(SIG_SETMASK, &old, 0) < 0) { perror("sigprocmask") ; exit(1) ; }
    }

    /* spawn_path_merge: modif sets a var the executed shell observes. */
    {
        char const *argv[] = { "/bin/sh", "-c", "case \"$FOO\" in bar) exit 0 ;; *) exit 3 ;; esac", 0 } ;
        char const modif[] = "FOO=bar" ;   /* trailing '\0' included by the literal */
        pid_t pid = spawn_path_merge("/bin/sh", argv, baseenv, modif, sizeof(modif)) ;
        TEST_ASSERT(pid > 0, "spawn_path_merge returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 0, "modif sets FOO=bar") ;
    }

    /* spawn_path_merge: modif replaces an existing var. */
    {
        char const *argv[] = { "/bin/sh", "-c", "case \"$FOO\" in bar) exit 0 ;; *) exit 3 ;; esac", 0 } ;
        char const *const env[] = { "PATH=/usr/bin:/bin", "FOO=old", 0 } ;
        char const modif[] = "FOO=bar" ;
        pid_t pid = spawn_path_merge("/bin/sh", argv, env, modif, sizeof(modif)) ;
        TEST_ASSERT(pid > 0, "spawn_path_merge returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 0, "modif replaces FOO") ;
    }

    /* spawn_path_merge: a key without '=' unsets the var. */
    {
        char const *argv[] = { "/bin/sh", "-c", "test -z \"${FOO+set}\" && exit 0 ; exit 3", 0 } ;
        char const *const env[] = { "PATH=/usr/bin:/bin", "FOO=bar", 0 } ;
        char const modif[] = "FOO" ;
        pid_t pid = spawn_path_merge("/bin/sh", argv, env, modif, sizeof(modif)) ;
        TEST_ASSERT(pid > 0, "spawn_path_merge returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 0, "bare key unsets FOO") ;
    }

    /** spawn_path_full MOVE: route the child's stdout into a pipe. The MOVE
     * dup2(p[1] -> 1) + close(p[1]) happens in the child; the parent reads the
     * echoed bytes back. Exercises file-actions ordering and dup2. */
    {
        int p[2] ;
        if (pipe(p) < 0) { perror("pipe") ; exit(1) ; }
        spawn_fa_t fa[] = { { SPAWN_FA_MOVE, p[1], 1, 0, 0, 0 } } ;
        char const *argv[] = { "/bin/sh", "-c", "echo hello", 0 } ;
        pid_t pid = spawn_path_full("/bin/sh", argv, baseenv, 0, 0, 0, fa, 1) ;
        TEST_ASSERT(pid > 0, "spawn_path_full MOVE returns a pid") ;
        close(p[1]) ;   // parent's copy; the child holds the write end as fd 1
        char buf[16] = {0} ;
        ssize_t r = read(p[0], buf, sizeof(buf) - 1) ;
        TEST_ASSERT(r == 6, "child's stdout routed to the pipe (6 bytes)") ;
        TEST_ASSERT(!memcmp(buf, "hello\n", 6), "child wrote 'hello\\n' to fd 1") ;
        close(p[0]) ;
        TEST_ASSERT(waitcode(pid) == 0, "echo exits 0") ;
    }

    /** spawn_path_full MOVE with from == to: the close must be skipped, else it
     * closes the fd just placed. Route fd 1 to the pipe (DUP2 p[1]->1), drop the
     * inherited pipe ends, then MOVE 1->1. If the no-op close were not skipped,
     * fd 1 would be closed and echo would write to a dead fd: the parent would
     * see EOF (0 bytes) instead of "hello". */
    {
        int p[2] ;
        if (pipe(p) < 0) { perror("pipe") ; exit(1) ; }
        spawn_fa_t fa[] = {
            { SPAWN_FA_DUP2,  p[1], 1, 0, 0, 0 },
            { SPAWN_FA_CLOSE, p[1], 0, 0, 0, 0 },
            { SPAWN_FA_CLOSE, p[0], 0, 0, 0, 0 },
            { SPAWN_FA_MOVE,  1,    1, 0, 0, 0 }
        } ;
        char const *argv[] = { "/bin/sh", "-c", "echo hello", 0 } ;
        pid_t pid = spawn_path_full("/bin/sh", argv, baseenv, 0, 0, 0, fa, 4) ;
        TEST_ASSERT(pid > 0, "spawn_path_full MOVE from==to returns a pid") ;
        close(p[1]) ;
        char buf[16] = {0} ;
        ssize_t r = read(p[0], buf, sizeof(buf) - 1) ;
        TEST_ASSERT(r == 6, "MOVE from==to kept fd 1 open (echo wrote 6 bytes)") ;
        TEST_ASSERT(!memcmp(buf, "hello\n", 6), "child wrote 'hello\\n'") ;
        close(p[0]) ;
        TEST_ASSERT(waitcode(pid) == 0, "echo exits 0") ;
    }

    /** spawn_path_full rejects unknown flag bits with 0 + EINVAL, no child. */
    {
        char const *argv[] = { "/bin/true", 0 } ;
        errno = 0 ;
        pid_t pid = spawn_path_full("/bin/true", argv, baseenv, 0, 0, 0x02, 0, 0) ;
        TEST_ASSERT(pid == 0, "unknown flag returns 0") ;
        TEST_ASSERT(errno == EINVAL, "unknown flag sets errno = EINVAL") ;
    }

    /** spawn_path_full SETSID: with SPAWN_FLAG_SETSID the child is its own
     * session leader (getsid(pid) == pid) and has left the parent's session.
     * The child blocks on stdin (a pipe the parent keeps open) so the getsid
     * checks are race-free; closing the write end then releases it. DUP2 of the
     * pipe read end onto fd 0 is what makes the child block. */
    {
        int p[2] ;
        if (pipe(p) < 0) { perror("pipe") ; exit(1) ; }
        /** Close the inherited write end in the child and put the read end on fd
         * 0: otherwise the child keeps a copy of the write end and its own read
         * never sees EOF. */
        spawn_fa_t fa[] = { { SPAWN_FA_CLOSE, p[1], 0, 0, 0, 0 }, { SPAWN_FA_DUP2, p[0], 0, 0, 0, 0 } } ;
        char const *argv[] = { "/bin/sh", "-c", "read x ; exit 0", 0 } ;
        pid_t pid = spawn_path_full("/bin/sh", argv, baseenv, 0, 0, SPAWN_FLAG_SETSID, fa, 2) ;
        TEST_ASSERT(pid > 0, "spawn_path_full SETSID returns a pid") ;
        TEST_ASSERT(getsid(pid) == pid, "SETSID: child is its own session leader") ;
        TEST_ASSERT(getsid(pid) != getsid(0), "SETSID: child left the parent's session") ;
        close(p[1]) ;   // EOF on the child's stdin -> read returns -> child exits
        close(p[0]) ;
        TEST_ASSERT(waitcode(pid) == 0, "child exits 0 after stdin EOF") ;
    }

    // spawn_path_full without SETSID: the child stays in the parent's session.
    {
        int p[2] ;
        if (pipe(p) < 0) { perror("pipe") ; exit(1) ; }
        spawn_fa_t fa[] = { { SPAWN_FA_CLOSE, p[1], 0, 0, 0, 0 }, { SPAWN_FA_DUP2, p[0], 0, 0, 0, 0 } } ;
        char const *argv[] = { "/bin/sh", "-c", "read x ; exit 0", 0 } ;
        pid_t pid = spawn_path_full("/bin/sh", argv, baseenv, 0, 0, 0, fa, 2) ;
        TEST_ASSERT(pid > 0, "spawn_path_full no-setsid returns a pid") ;
        TEST_ASSERT(getsid(pid) == getsid(0), "no SETSID: child stays in the parent's session") ;
        close(p[1]) ;
        close(p[0]) ;
        TEST_ASSERT(waitcode(pid) == 0, "child exits 0") ;
    }

    /** spawn_path_full OPEN: open /dev/null onto the child's fd 1; the child's
     * write succeeds and it exits 0. */
    {
        spawn_fa_t fa[] = { { SPAWN_FA_OPEN, 0, 1, "/dev/null", O_WRONLY, 0 } } ;
        char const *argv[] = { "/bin/sh", "-c", "echo x", 0 } ;
        pid_t pid = spawn_path_full("/bin/sh", argv, baseenv, 0, 0, 0, fa, 1) ;
        TEST_ASSERT(pid > 0, "spawn_path_full OPEN returns a pid") ;
        TEST_ASSERT(waitcode(pid) == 0, "child writes to /dev/null, exits 0") ;
    }

    /** spawn_path_full: an unknown file-action type fails with 0 + EINVAL, and
     * no child is created. */
    {
        spawn_fa_t fa[] = { { 99, 0, 0, 0, 0, 0 } } ;
        char const *argv[] = { "/bin/true", 0 } ;
        errno = 0 ;
        pid_t pid = spawn_path_full("/bin/true", argv, baseenv, 0, 0, 0, fa, 1) ;
        TEST_ASSERT(pid == 0, "invalid file-action returns 0") ;
        TEST_ASSERT(errno == EINVAL, "invalid file-action sets errno = EINVAL") ;
    }

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
