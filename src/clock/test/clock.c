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

/* Tests for the oblibs clock module (tai/djbtime replacement for log stamps).
 * The expected TAI64N '@hex' and local ISO strings were CAPTURED from a
 * byte-exact differential run against skalibs (timestamp_fmt / localtmn_fmt)
 * and are now pinned as literals, so the test is self-contained (no skalibs).
 * TZ is forced to UTC so the local stamps are deterministic. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <oblibs/clock.h>

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

/* Golden table captured from skalibs (offset +37, == our CLOCK_TAI_UTC). */
typedef struct gold_s { time_t sec ; long nano ; char const *tai ; char const *local ; } gold_t ;

static gold_t const gold[] = {
    { 1483228800, 0,         "@40000000586846a500000000", "2017-01-01 00:00:00.000000000" },
    { 1483228800, 1,         "@40000000586846a500000001", "2017-01-01 00:00:00.000000001" },
    { 1483228800, 123456789, "@40000000586846a5075bcd15", "2017-01-01 00:00:00.123456789" },
    { 1483228800, 999999999, "@40000000586846a53b9ac9ff", "2017-01-01 00:00:00.999999999" },
    { 1483228800, 500000000, "@40000000586846a51dcd6500", "2017-01-01 00:00:00.500000000" },
    { 1700000000, 0,         "@400000006553f12500000000", "2023-11-14 22:13:20.000000000" },
    { 1700000000, 1,         "@400000006553f12500000001", "2023-11-14 22:13:20.000000001" },
    { 1700000000, 123456789, "@400000006553f125075bcd15", "2023-11-14 22:13:20.123456789" },
    { 1700000000, 999999999, "@400000006553f1253b9ac9ff", "2023-11-14 22:13:20.999999999" },
    { 1700000000, 500000000, "@400000006553f1251dcd6500", "2023-11-14 22:13:20.500000000" },
    { 2000000000, 0,         "@400000007735942500000000", "2033-05-18 03:33:20.000000000" },
    { 2000000000, 1,         "@400000007735942500000001", "2033-05-18 03:33:20.000000001" },
    { 2000000000, 123456789, "@4000000077359425075bcd15", "2033-05-18 03:33:20.123456789" },
    { 2000000000, 999999999, "@40000000773594253b9ac9ff", "2033-05-18 03:33:20.999999999" },
    { 2000000000, 500000000, "@40000000773594251dcd6500", "2033-05-18 03:33:20.500000000" },
} ;

static void test_golden(void)
{
    TEST_START("test_golden") ;

    for (size_t i = 0 ; i < sizeof gold / sizeof *gold ; i++) {
        struct timespec ts = { .tv_sec = gold[i].sec, .tv_nsec = gold[i].nano } ;

        char a[CLOCK_TAI64N_LEN + 1] ;
        size_t la = clock_tai64n_fmt(a, &ts) ;
        TEST_ASSERT(la == CLOCK_TAI64N_LEN, "tai64n length") ;
        TEST_ASSERT(!strcmp(a, gold[i].tai), "tai64n stamp matches captured skalibs output") ;

        char d[CLOCK_LOCAL_LEN + 1] ;
        size_t ld = clock_local_fmt(d, &ts) ;
        TEST_ASSERT(ld == CLOCK_LOCAL_LEN, "local length") ;
        TEST_ASSERT(!strcmp(d, gold[i].local), "local stamp matches captured skalibs output") ;
    }

    TEST_END() ;
}

/* clock_now must succeed and produce well-formed, self-consistent stamps. */
static void test_now(void)
{
    TEST_START("test_now") ;

    struct timespec now ;
    TEST_ASSERT(clock_now(&now), "clock_now") ;

    char a[CLOCK_TAI64N_LEN + 1] ;
    TEST_ASSERT(clock_tai64n_fmt(a, &now) == CLOCK_TAI64N_LEN, "now tai64n length") ;
    TEST_ASSERT(a[0] == '@', "now tai64n starts with @") ;

    char d[CLOCK_LOCAL_LEN + 1] ;
    TEST_ASSERT(clock_local_fmt(d, &now) == CLOCK_LOCAL_LEN, "now local length") ;

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

    test_golden() ;
    test_now() ;
    test_constant() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
