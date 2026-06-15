/*
 * log_fmt.c
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

/* Tests for the formatted logging layer: the log_fmt() helper and the flog_*
 * macros. flog_* output goes through the log engine to fd 2, so the non-fatal
 * macros are exercised by redirecting fd 2 to a temp file and reading it back.
 * The fatal flog_die* macros call _exit() and are not tested here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/log.h>
#include <oblibs/files.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)) ; \
            exit(1) ; \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

/* log_fmt formats into the caller buffer and returns it. */
static void test_log_fmt(void)
{
    TEST_START("test_log_fmt") ;

    char buf[64] ;
    char *r = log_fmt(buf, sizeof buf, "%s=%d;%u", "x", -3, 7u) ;
    TEST_ASSERT(r == buf, "returns the buffer") ;
    TEST_ASSERT(strcmp(buf, "x=-3;7") == 0, "formatted content") ;

    /* truncation is bounded by n (no overflow) */
    char small[4] ;
    log_fmt(small, sizeof small, "%s", "abcdefgh") ;
    TEST_ASSERT(strcmp(small, "abc") == 0, "truncated to n-1") ;

    TEST_END() ;
}

/* Capture fd 2 into a temp file across a single flog_* call, return the text. */
static void capture_flog(char *out, size_t outn, void (*emit)(void))
{
    char path[] = "/tmp/oblibs_flog_XXXXXX" ;
    int tmp = mkstemp(path) ;
    TEST_ASSERT(tmp >= 0, "mkstemp") ;
    file_tryunlink(path) ;

    int saved = dup(2) ;
    TEST_ASSERT(saved >= 0, "dup(2)") ;
    TEST_ASSERT(dup2(tmp, 2) >= 0, "dup2 onto fd 2") ;

    emit() ;

    fsync(2) ;
    TEST_ASSERT(dup2(saved, 2) >= 0, "restore fd 2") ;
    close(saved) ;

    TEST_ASSERT(lseek(tmp, 0, SEEK_SET) == 0, "lseek") ;
    ssize_t r = read(tmp, out, outn - 1) ;
    TEST_ASSERT(r >= 0, "read back") ;
    out[r] = 0 ;
    close(tmp) ;
}

static void emit_warn(void) { flog_warn("count=%d done", 42) ; }

/* A non-fatal flog_* call produces the formatted payload on fd 2. */
static void test_flog_warn(void)
{
    TEST_START("test_flog_warn") ;

    PROG = "logtest" ;
    VERBOSITY = 3 ;

    char got[256] ;
    capture_flog(got, sizeof got, emit_warn) ;

    TEST_ASSERT(strstr(got, "warning") != NULL, "has the warning tag") ;
    TEST_ASSERT(strstr(got, "count=42 done") != NULL, "has the formatted payload") ;

    TEST_END() ;
}

int main(void)
{
    printf("=== log_fmt / flog test suite ===\n\n") ;

    test_log_fmt() ;
    test_flog_warn() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
