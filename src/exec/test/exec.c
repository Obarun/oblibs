/*
 * exec.c
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
 * Behavioural test for the exec_path family. Each case forks; the child
 * performs the exec and the parent inspects the exit status. Process-image
 * replacement cannot be unit-tested in-process, hence the fork harness.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <oblibs/exec.h>

#define EXEC_RETURNED 120   /* sentinel: a non-dying exec returned (i.e. failed) */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

/* PATH known to contain true/sh on every supported system. */
static char const *const baseenv[] = { "PATH=/usr/bin:/bin", 0 } ;

static int waitstatus (pid_t pid)
{
    int wstat ;
    if (waitpid(pid, &wstat, 0) < 0) { perror("waitpid") ; exit(1) ; }
    return WIFEXITED(wstat) ? WEXITSTATUS(wstat) : -1 ;
}

/* exec_path: child runs it, _exit(EXEC_RETURNED) if it comes back. */
static int run_path (char const *file, char const *const *argv, char const *const *envp)
{
    pid_t pid = fork() ;
    if (pid < 0) { perror("fork") ; exit(1) ; }
    if (!pid) { exec_path(file, argv, envp) ; _exit(EXEC_RETURNED) ; }
    return waitstatus(pid) ;
}

/* exec_path_merge: same harness with a modif string. */
static int run_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen)
{
    pid_t pid = fork() ;
    if (pid < 0) { perror("fork") ; exit(1) ; }
    if (!pid) { exec_path_merge(file, argv, envp, modif, modiflen) ; _exit(EXEC_RETURNED) ; }
    return waitstatus(pid) ;
}

/* exec_path_die: child stderr silenced (the die path logs). Never returns. */
static int run_path_die (char const *file, char const *const *argv, char const *const *envp)
{
    pid_t pid = fork() ;
    if (pid < 0) { perror("fork") ; exit(1) ; }
    if (!pid) {
        int n = open("/dev/null", O_WRONLY) ;
        if (n >= 0) { dup2(n, 2) ; close(n) ; }
        exec_path_die(file, argv, envp) ;
        _exit(EXEC_RETURNED) ;
    }
    return waitstatus(pid) ;
}

int main (void)
{
    printf("Running exec...") ;

    /* exec_path: absolute path, no PATH search. */
    {
        char const *argv[] = { "/bin/true", 0 } ;
        TEST_ASSERT(run_path("/bin/true", argv, baseenv) == 0, "absolute /bin/true exits 0") ;
    }
    {
        char const *argv[] = { "/bin/false", 0 } ;
        TEST_ASSERT(run_path("/bin/false", argv, baseenv) == 1, "absolute /bin/false exits 1") ;
    }

    /* exec_path: bare name resolved through getenv("PATH"). */
    {
        char const *argv[] = { "true", 0 } ;
        setenv("PATH", "/usr/bin:/bin", 1) ;
        TEST_ASSERT(run_path("true", argv, baseenv) == 0, "bare 'true' found via PATH") ;
    }

    /* exec_path: failures return (sentinel), errno is irrelevant here. */
    {
        char const *argv[] = { "/nonexistent/nope", 0 } ;
        TEST_ASSERT(run_path("/nonexistent/nope", argv, baseenv) == EXEC_RETURNED, "missing file returns") ;
    }

    /* exec_path_die: ENOENT -> 127, other (here EISDIR/EACCES on '/') -> 126. */
    {
        char const *argv[] = { "/nonexistent/nope", 0 } ;
        TEST_ASSERT(run_path_die("/nonexistent/nope", argv, baseenv) == 127, "missing -> 127") ;
    }
    {
        char const *argv[] = { "/", 0 } ;
        TEST_ASSERT(run_path_die("/", argv, baseenv) == 126, "not executable -> 126") ;
    }

    /* exec_path_merge: modif sets a var the executed shell observes. */
    {
        char const *argv[] = { "/bin/sh", "-c", "case \"$FOO\" in bar) exit 0 ;; *) exit 3 ;; esac", 0 } ;
        char const modif[] = "FOO=bar" ;   /* trailing '\0' included by the literal */
        TEST_ASSERT(run_merge("/bin/sh", argv, baseenv, modif, sizeof(modif)) == 0, "modif sets FOO=bar") ;
    }

    /* exec_path_merge: modif replaces an existing var. */
    {
        char const *argv[] = { "/bin/sh", "-c", "case \"$FOO\" in bar) exit 0 ;; *) exit 3 ;; esac", 0 } ;
        char const *const env[] = { "PATH=/usr/bin:/bin", "FOO=old", 0 } ;
        char const modif[] = "FOO=bar" ;
        TEST_ASSERT(run_merge("/bin/sh", argv, env, modif, sizeof(modif)) == 0, "modif replaces FOO") ;
    }

    /* exec_path_merge: a key without '=' unsets the var. */
    {
        char const *argv[] = { "/bin/sh", "-c", "test -z \"${FOO+set}\" && exit 0 ; exit 3", 0 } ;
        char const *const env[] = { "PATH=/usr/bin:/bin", "FOO=bar", 0 } ;
        char const modif[] = "FOO" ;
        TEST_ASSERT(run_merge("/bin/sh", argv, env, modif, sizeof(modif)) == 0, "bare key unsets FOO") ;
    }

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
