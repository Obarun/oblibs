/*
 * types.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
 * Differential test for uid_t / gid_t / pid_t conversions (oblibs/types.h),
 * compared against the equivalent skalibs engine. Any mismatch fails.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include <oblibs/types.h>

#include <skalibs/uint64.h>   /* uint64_fmt_generic, int64_fmt_generic, uint64_scan_base_max, int64_scan_base_max */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            printf("      Line %d: %s\n", __LINE__, #condition); \
            exit(1); \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

static uint64_t lcg_state = 0x0f1e2d3c4b5a6978ULL ;
static uint64_t lcg_next(void)
{
    lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL ;
    return lcg_state ;
}

#define UID_MAX_VAL ((uint64_t)(uid_t)-1)
#define GID_MAX_VAL ((uint64_t)(gid_t)-1)
#define PID_MAX_VAL (((uint64_t)1 << (sizeof(pid_t)*8 - 1)) - 1)

/* uid_t (unsigned, capped at its max) */
static void check_uid(uid_t u, uint8_t base)
{
    char a[64], b[64] ;
    size_t la = uid_format_base(a, u, base) ;
    size_t lb = uint64_fmt_generic(b, (uint64_t)u, base) ;
    TEST_ASSERT(la == lb && !memcmp(a, b, la), "uid_format differs from skalibs") ;
}
static void check_uid_scan(char const *s, uint8_t base)
{
    uid_t a = (uid_t)0xa5a5a5a5u ;
    uint64_t ref = 0 ;
    size_t pa = uid_parse_base(s, &a, base) ;
    size_t pb = uint64_scan_base_max(s, &ref, base, UID_MAX_VAL) ;
    TEST_ASSERT(pa == pb, "uid_parse position differs from skalibs (capped)") ;
    if (pa) TEST_ASSERT(a == (uid_t)ref, "uid_parse value differs from skalibs (capped)") ;
}

/* gid_t (unsigned, capped at its max) */
static void check_gid(gid_t g, uint8_t base)
{
    char a[64], b[64] ;
    size_t la = gid_format_base(a, g, base) ;
    size_t lb = uint64_fmt_generic(b, (uint64_t)g, base) ;
    TEST_ASSERT(la == lb && !memcmp(a, b, la), "gid_format differs from skalibs") ;
}
static void check_gid_scan(char const *s, uint8_t base)
{
    gid_t a = (gid_t)0xa5a5a5a5u ;
    uint64_t ref = 0 ;
    size_t pa = gid_parse_base(s, &a, base) ;
    size_t pb = uint64_scan_base_max(s, &ref, base, GID_MAX_VAL) ;
    TEST_ASSERT(pa == pb, "gid_parse position differs from skalibs (capped)") ;
    if (pa) TEST_ASSERT(a == (gid_t)ref, "gid_parse value differs from skalibs (capped)") ;
}

/* pid_t (signed, strict signed range) */
static void check_pid(pid_t p, uint8_t base)
{
    char a[64], b[64] ;
    size_t la = pid_format_base(a, p, base) ;
    size_t lb = int64_fmt_generic(b, (int64_t)p, base) ;
    TEST_ASSERT(la == lb && !memcmp(a, b, la), "pid_format differs from skalibs") ;
}
static void check_pid_scan(char const *s, uint8_t base)
{
    pid_t a = (pid_t)0x5a5a5a5a ;
    int64_t ref = 0 ;
    size_t pa = pid_parse_base(s, &a, base) ;
    size_t pb = int64_scan_base_max(s, &ref, base, PID_MAX_VAL) ;
    TEST_ASSERT(pa == pb, "pid_parse position differs from skalibs (capped)") ;
    if (pa) TEST_ASSERT(a == (pid_t)ref, "pid_parse value differs from skalibs (capped)") ;
}

static void test_fmt(void)
{
    TEST_START("test_fmt") ;
    /* size constants must hold the worst case + terminator */
    TEST_ASSERT(uid_format_base(0, (uid_t)UID_MAX_VAL, 10) + 1 <= UID_FMT, "UID_FMT too small") ;
    TEST_ASSERT(gid_format_base(0, (gid_t)GID_MAX_VAL, 10) + 1 <= GID_FMT, "GID_FMT too small") ;
    TEST_ASSERT(pid_format_base(0, (pid_t)-PID_MAX_VAL - 1, 10) + 1 <= PID_FMT, "PID_FMT too small") ;

    for (uint8_t base = 2 ; base <= 36 ; base++) {
        for (long i = 0 ; i < 20000 ; i++) {
            uint64_t r = lcg_next() ;
            check_uid((uid_t)r, base) ;
            check_gid((gid_t)r, base) ;
            check_pid((pid_t)r, base) ;
        }
        check_uid((uid_t)0, base) ; check_uid((uid_t)UID_MAX_VAL, base) ;
        check_pid((pid_t)0, base) ; check_pid((pid_t)1, base) ;
        check_pid((pid_t)-1, base) ; check_pid((pid_t)PID_MAX_VAL, base) ;
        check_pid((pid_t)(-PID_MAX_VAL - 1), base) ;
    }
    TEST_END() ;
}

static void test_scan(void)
{
    TEST_START("test_scan") ;
    /* round-trip in range */
    for (uint8_t base = 2 ; base <= 36 ; base++) {
        for (long i = 0 ; i < 20000 ; i++) {
            uint64_t r = lcg_next() ;
            char buf[64] ; size_t len ;
            len = uid_format_base(buf, (uid_t)r, base) ; buf[len] = 0 ; check_uid_scan(buf, base) ;
            len = gid_format_base(buf, (gid_t)r, base) ; buf[len] = 0 ; check_gid_scan(buf, base) ;
            len = pid_format_base(buf, (pid_t)r, base) ; buf[len] = 0 ; check_pid_scan(buf, base) ;
        }
    }
    /* boundary / out-of-range / invalid (freezes the capping semantics) */
    static char const *str[] = {
        "", " ", "z", "-1", "+1", "-",
        "0", "255", "256", "65535", "65536",
        "2147483647", "2147483648", "-2147483648", "-2147483649",
        "4294967295", "4294967296", "5000000000",
        "9223372036854775807", "18446744073709551616",
        "ff", "-ff", "100000000"
    } ;
    for (size_t i = 0 ; i < sizeof(str)/sizeof(*str) ; i++)
        for (uint8_t base = 2 ; base <= 36 ; base++) {
            check_uid_scan(str[i], base) ;
            check_gid_scan(str[i], base) ;
            check_pid_scan(str[i], base) ;
        }
    TEST_END() ;
}

int main(void)
{
    printf("=== uid/gid/pid conversion test vs skalibs ===\n") ;
    test_fmt() ;
    test_scan() ;
    printf("All types tests passed.\n") ;
    return 0 ;
}
