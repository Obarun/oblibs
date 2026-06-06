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
#include <stdint.h>
#include <sys/time.h>

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

/* ---- clock_pack / clock_unpack -------------------------------------------
 * Golden bytes captured from a byte-exact differential against skalibs
 * tain_pack (relative tain), plus the round-trip identity. */
typedef struct packgold_s { time_t sec ; long nano ; char const bytes[CLOCK_PACK] ; } packgold_t ;

static packgold_t const packgold[] = {
    { 0,          0,         { 0,0,0,0, 0,0,0,0,    0,0,0,0 } },
    { 3,          0,         { 0,0,0,0, 0,0,0,0x03, 0,0,0,0 } },
    { 600,        0,         { 0,0,0,0, 0,0,0x02,0x58, 0,0,0,0 } },
    { 1,          500000000, { 0,0,0,0, 0,0,0,0x01, 0x1d,0xcd,0x65,0x00 } },
    { 1700000000, 123456789, { 0,0,0,0, 0x65,0x53,0xf1,0x00, 0x07,0x5b,0xcd,0x15 } },
    { 1700000000, 999999999, { 0,0,0,0, 0x65,0x53,0xf1,0x00, 0x3b,0x9a,0xc9,0xff } },
} ;

static void test_pack(void)
{
    TEST_START("test_pack") ;

    for (size_t i = 0 ; i < sizeof packgold / sizeof *packgold ; i++) {
        struct timespec ts = { .tv_sec = packgold[i].sec, .tv_nsec = packgold[i].nano } ;
        char b[CLOCK_PACK] ;
        clock_pack(b, &ts) ;
        TEST_ASSERT(!memcmp(b, packgold[i].bytes, CLOCK_PACK), "packed bytes match golden") ;

        struct timespec rt ;
        clock_unpack(b, &rt) ;
        TEST_ASSERT(rt.tv_sec == ts.tv_sec && rt.tv_nsec == ts.tv_nsec, "unpack round-trips pack") ;
    }

    TEST_END() ;
}

/* ---- clock_to_timeval ----------------------------------------------------
 * Golden {tv_sec, tv_usec} captured byte-exact vs skalibs timeval_from_tain.
 * Note entries 5/6: nano >= 999999500 rounds tv_usec to 1000000 un-normalised,
 * matching skalibs exactly (see clock_to_timeval comment). */
typedef struct tvgold_s { time_t sec ; long nano ; long tv_sec ; long tv_usec ; } tvgold_t ;

static tvgold_t const tvgold[] = {
    { 0,          0,         0,          0 },
    { 600,        0,         600,        0 },
    { 1,          500000000, 1,          500000 },
    { 1700000000, 123456789, 1700000000, 123457 },
    { 1700000000, 999999999, 1700000000, 1000000 },
    { 1700000000, 999999500, 1700000000, 1000000 },
    { 1700000000, 999999499, 1700000000, 999999 },
} ;

static void test_timeval(void)
{
    TEST_START("test_timeval") ;

    for (size_t i = 0 ; i < sizeof tvgold / sizeof *tvgold ; i++) {
        struct timespec ts = { .tv_sec = tvgold[i].sec, .tv_nsec = tvgold[i].nano } ;
        struct timeval tv ;
        clock_to_timeval(&tv, &ts) ;
        TEST_ASSERT((long)tv.tv_sec == tvgold[i].tv_sec, "tv_sec matches golden") ;
        TEST_ASSERT((long)tv.tv_usec == tvgold[i].tv_usec, "tv_usec matches golden") ;
    }

    TEST_END() ;
}

/* ---- clock_to_localtm / clock_from_localtm -------------------------------
 * Broken-down fields captured byte-exact vs skalibs localtm_from_tai, under
 * two zones. Assemble (clock_from_localtm) is checked by libc round-trip, the
 * exact inverse of localtime_r. */
typedef struct calgold_s { time_t sec ; int year, mon, mday, hour, min, s, wday, yday ; } calgold_t ;

static calgold_t const cal_utc[] = {
    { 0,          1970, 1, 1,  0,  0,  0, 4, 0 },
    { 1483228800, 2017, 1, 1,  0,  0,  0, 0, 0 },
    { 951782400,  2000, 2, 29, 0,  0,  0, 2, 59 },
    { 1700000000, 2023, 11, 14, 22, 13, 20, 2, 317 },
    { 1735689599, 2024, 12, 31, 23, 59, 59, 2, 365 },
} ;

static calgold_t const cal_noumea[] = {
    { 0,          1970, 1, 1,  11, 0,  0, 4, 0 },
    { 1483228800, 2017, 1, 1,  11, 0,  0, 0, 0 },
    { 951782400,  2000, 2, 29, 11, 0,  0, 2, 59 },
    { 1700000000, 2023, 11, 15, 9,  13, 20, 3, 318 },
    { 1735689599, 2025, 1, 1,  10, 59, 59, 3, 0 },
} ;

static void check_cal(calgold_t const *g, size_t n)
{
    for (size_t i = 0 ; i < n ; i++) {
        struct timespec ts = { .tv_sec = g[i].sec, .tv_nsec = 0 } ;
        struct tm tm ;
        memset(&tm, 0, sizeof tm) ;
        TEST_ASSERT(clock_to_localtm(&tm, &ts), "clock_to_localtm") ;
        TEST_ASSERT(1900 + tm.tm_year == g[i].year, "year") ;
        TEST_ASSERT(1 + tm.tm_mon == g[i].mon, "month") ;
        TEST_ASSERT(tm.tm_mday == g[i].mday, "mday") ;
        TEST_ASSERT(tm.tm_hour == g[i].hour, "hour") ;
        TEST_ASSERT(tm.tm_min == g[i].min, "min") ;
        TEST_ASSERT(tm.tm_sec == g[i].s, "sec") ;
        TEST_ASSERT(tm.tm_wday == g[i].wday, "wday") ;
        TEST_ASSERT(tm.tm_yday == g[i].yday, "yday") ;

        struct tm tm2 = tm ;
        struct timespec back ;
        TEST_ASSERT(clock_from_localtm(&back, &tm2), "clock_from_localtm") ;
        TEST_ASSERT(back.tv_sec == g[i].sec && back.tv_nsec == 0, "assemble round-trips break-down") ;
    }
}

static void test_calendar(void)
{
    TEST_START("test_calendar") ;

    setenv("TZ", "UTC", 1) ; tzset() ;
    check_cal(cal_utc, sizeof cal_utc / sizeof *cal_utc) ;

    setenv("TZ", "Pacific/Noumea", 1) ; tzset() ;
    check_cal(cal_noumea, sizeof cal_noumea / sizeof *cal_noumea) ;

    setenv("TZ", "UTC", 1) ; tzset() ;   /* restore deterministic zone */

    TEST_END() ;
}

/* ---- arithmetic ----------------------------------------------------------*/
static void test_arith(void)
{
    TEST_START("test_arith") ;

    struct timespec a = { 100, 800000000L }, b = { 5, 700000000L }, r ;

    clock_add(&r, &a, &b) ;
    TEST_ASSERT(r.tv_sec == 106 && r.tv_nsec == 500000000L, "add with nano carry") ;

    clock_sub(&r, &a, &b) ;
    TEST_ASSERT(r.tv_sec == 95 && r.tv_nsec == 100000000L, "sub") ;

    clock_sub(&r, &b, &a) ;   /* 5.7 - 100.8 = -95.1 -> {-96, 9e8} */
    TEST_ASSERT(r.tv_sec == -96 && r.tv_nsec == 900000000L, "sub with borrow (negative)") ;

    clock_addsec(&r, &a, -50) ;
    TEST_ASSERT(r.tv_sec == 50 && r.tv_nsec == 800000000L, "addsec negative") ;

    clock_addsec(&r, &a, 5) ;
    TEST_ASSERT(r.tv_sec == 105 && r.tv_nsec == 800000000L, "addsec positive") ;

    clock_from_ms(&r, 3500) ;
    TEST_ASSERT(r.tv_sec == 3 && r.tv_nsec == 500000000L, "from_ms") ;

    TEST_ASSERT(clock_cmp(&a, &b) > 0, "cmp a>b") ;
    TEST_ASSERT(clock_cmp(&b, &a) < 0, "cmp b<a") ;
    TEST_ASSERT(clock_cmp(&a, &a) == 0, "cmp equal") ;

    TEST_END() ;
}

/* monotonic clock: succeeds and never goes backward. */
static void test_mono(void)
{
    TEST_START("test_mono") ;

    struct timespec t1, t2 ;
    TEST_ASSERT(clock_now_mono(&t1), "clock_now_mono") ;
    TEST_ASSERT(clock_now_mono(&t2), "clock_now_mono") ;
    TEST_ASSERT(clock_cmp(&t2, &t1) >= 0, "monotonic non-decreasing") ;

    TEST_END() ;
}

/* deepsleep: a 50ms monotonic deadline returns at or after the deadline. */
static void test_deepsleep(void)
{
    TEST_START("test_deepsleep") ;

    struct timespec start, deadline, end, delay = { 0, 50000000L } ;
    TEST_ASSERT(clock_now_mono(&start), "clock_now_mono") ;
    clock_add(&deadline, &start, &delay) ;
    TEST_ASSERT(clock_deepsleep(&deadline), "clock_deepsleep") ;
    TEST_ASSERT(clock_now_mono(&end), "clock_now_mono") ;
    TEST_ASSERT(clock_cmp(&end, &deadline) >= 0, "slept until at least the deadline") ;

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
    test_pack() ;
    test_timeval() ;
    test_calendar() ;
    test_arith() ;
    test_mono() ;
    test_deepsleep() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
