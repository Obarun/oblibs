/*
 * process_wait.c
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
 * Behavioural test for process_wait. Each case forks a real child and the parent
 * reaps it with process_wait, inspecting the returned pid and the wait status.
 * No dependency on the spawn module: plain fork()/_exit().
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include <oblibs/process.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

int main (void)
{
    printf("Running process_wait...") ;

    /* child exits normally: pid reaped, exit status propagated. */
    {
        pid_t pid = fork() ;
        if (pid < 0) { perror("fork") ; exit(1) ; }
        if (!pid) _exit(42) ;

        int wstat = 0 ;
        pid_t r = process_wait(pid, &wstat) ;
        TEST_ASSERT(r == pid, "process_wait returns the reaped pid") ;
        TEST_ASSERT(WIFEXITED(wstat), "child exited normally") ;
        TEST_ASSERT(WEXITSTATUS(wstat) == 42, "exit status is propagated") ;
    }

    /* child killed by a signal: WIFSIGNALED reported. */
    {
        pid_t pid = fork() ;
        if (pid < 0) { perror("fork") ; exit(1) ; }
        if (!pid) { raise(SIGKILL) ; _exit(0) ; }

        int wstat = 0 ;
        pid_t r = process_wait(pid, &wstat) ;
        TEST_ASSERT(r == pid, "process_wait returns the reaped pid") ;
        TEST_ASSERT(WIFSIGNALED(wstat), "child was signaled") ;
        TEST_ASSERT(WTERMSIG(wstat) == SIGKILL, "the killing signal is reported") ;
    }

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
