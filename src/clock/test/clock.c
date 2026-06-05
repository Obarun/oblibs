/*
 * clock.c
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

/* Tests for the oblibs clock module, the tai/djbtime replacement used for log
 * timestamps. Differential against skalibs for recent times (>= 2017, where
 * skalibs' leap table yields the same +37 offset as our CLOCK_TAI_UTC constant):
 * both the TAI64N '@hex' stamp and the local ISO stamp must match skalibs
 * byte-for-byte. Plus a TZ=UTC fixed case for a fully deterministic assertion. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <oblibs/clock.h>

#include <skalibs/tai.h>       /* tain, tain_from_timespec_sysclock, timestamp_fmt, TIMESTAMP */
#include <skalibs/djbtime.h>   /* localtmn, localtmn_from_tain, localtmn_fmt, LOCALTMN_FMT */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

/* Compare oblibs vs skalibs formatting for a given (sec, nano), sec >= 2017. */
static void check(time_t sec, long nano)
{
    struct timespec ts ;
    ts.tv_sec = sec ;
    ts.tv_nsec = nano ;

    tain x ;
    TEST_ASSERT(tain_from_timespec_sysclock(&x, &ts), "skalibs tain build") ;

    char a[CLOCK_TAI64N_LEN + 1], b[TIMESTAMP + 1] ;
    clock_tai64n_fmt(a, &ts) ;
    size_t lb = timestamp_fmt(b, &x) ;
    b[lb] = 0 ;
    TEST_ASSERT(!strcmp(a, b), "TAI64N stamp matches skalibs") ;

    char d[CLOCK_LOCAL_LEN + 1], c[LOCALTMN_FMT + 1] ;
    clock_local_fmt(d, &ts) ;
    localtmn l ;
    localtmn_from_tain(&l, &x, 1) ;
    size_t lc = localtmn_fmt(c, &l) ;
    c[lc] = 0 ;
    TEST_ASSERT(!strcmp(d, c), "local stamp matches skalibs") ;
}

/* Deterministic, TZ-independent: known UTC instant -> exact string. */
static void test_fixed_utc(void)
{
    TEST_START("test_fixed_utc") ;

    struct timespec ts ;
    ts.tv_sec = 1700000000 ;   /* 2023-11-14 22:13:20 UTC */
    ts.tv_nsec = 123456789 ;

    char d[CLOCK_LOCAL_LEN + 1] ;
    size_t n = clock_local_fmt(d, &ts) ;
    TEST_ASSERT(n == CLOCK_LOCAL_LEN, "local length") ;
    TEST_ASSERT(!strcmp(d, "2023-11-14 22:13:20.123456789"), "fixed UTC local stamp") ;

    char a[CLOCK_TAI64N_LEN + 1] ;
    size_t m = clock_tai64n_fmt(a, &ts) ;
    TEST_ASSERT(m == CLOCK_TAI64N_LEN, "tai64n length") ;
    TEST_ASSERT(a[0] == '@', "tai64n starts with @") ;
    /* exact TAI64N bytes are covered byte-for-byte by test_differential vs skalibs */

    TEST_END() ;
}

static void test_differential(void)
{
    TEST_START("test_differential") ;

    static time_t const secs[] = { 1483228800, 1700000000, 1750000000, 2000000000 } ;
    static long const nanos[] = { 0, 1, 123456789, 999999999, 500000000 } ;

    for (size_t i = 0 ; i < sizeof secs / sizeof *secs ; i++)
        for (size_t j = 0 ; j < sizeof nanos / sizeof *nanos ; j++)
            check(secs[i], nanos[j]) ;

    /* and "now" */
    struct timespec now ;
    TEST_ASSERT(clock_now(&now), "clock_now") ;
    check(now.tv_sec, now.tv_nsec) ;

    TEST_END() ;
}

static void test_constant(void)
{
    TEST_START("test_constant") ;
    TEST_ASSERT(CLOCK_TAI_UTC == 37, "current TAI-UTC offset") ;
    TEST_END() ;
}

int main(void)
{
    setenv("TZ", "UTC", 1) ;
    tzset() ;

    printf("=== clock test suite ===\n\n") ;

    test_fixed_utc() ;
    test_differential() ;
    test_constant() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
