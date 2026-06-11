/*
 * spawn.c
 *
 * Copyright (c) 2018-2026 Eric Vidal <eric@obarun.org>
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

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
