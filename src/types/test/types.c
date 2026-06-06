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
 * Self-contained test for uid_t / gid_t / pid_t conversions (oblibs/types.h).
 * No skalibs. ROUND-TRIP (format then parse reproduces the value) proves the
 * fmt/parse pair over wide sweeps; GOLDEN anchors pin the capping / rejection
 * semantics at the type boundaries, captured from the original byte-exact
 * differential run against skalibs. uid_t and gid_t are unsigned (capped at
 * their type max); pid_t is signed (strict signed range).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include <oblibs/types.h>

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

/* ---- round-trip ---- */

static void rt(uint64_t r, uint8_t base)
{
    char buf[64] ; size_t len ;
    uid_t u = (uid_t)r, ou = ~u ;
    gid_t g = (gid_t)r, og = ~g ;
    pid_t p = (pid_t)r, op = ~p ;

    len = uid_format_base(buf, u, base) ; buf[len] = 0 ;
    TEST_ASSERT(uid_parse_base(buf, &ou, base) == len && ou == u, "uid round-trip") ;
    len = gid_format_base(buf, g, base) ; buf[len] = 0 ;
    TEST_ASSERT(gid_parse_base(buf, &og, base) == len && og == g, "gid round-trip") ;
    len = pid_format_base(buf, p, base) ; buf[len] = 0 ;
    TEST_ASSERT(pid_parse_base(buf, &op, base) == len && op == p, "pid round-trip") ;
}

static void test_roundtrip(void)
{
    TEST_START("test_roundtrip") ;
    for (uint8_t base = 2 ; base <= 36 ; base++) {
        for (long i = 0 ; i < 20000 ; i++)
            rt(lcg_next(), base) ;
        rt(0, base) ; rt(1, base) ; rt(UID_MAX_VAL, base) ;
        rt((uint64_t)(uint32_t)-1, base) ;          /* pid: -1 */
        rt(PID_MAX_VAL, base) ;
        rt((uint64_t)(uint32_t)(-(int64_t)PID_MAX_VAL - 1), base) ;   /* pid: INT32_MIN */
    }
    TEST_END() ;
}

/* ---- format size constants + a few golden fmt anchors ---- */

static void test_fmt(void)
{
    TEST_START("test_fmt") ;
    TEST_ASSERT(uid_format_base(0, (uid_t)UID_MAX_VAL, 10) + 1 <= UID_FMT, "UID_FMT too small") ;
    TEST_ASSERT(gid_format_base(0, (gid_t)GID_MAX_VAL, 10) + 1 <= GID_FMT, "GID_FMT too small") ;
    TEST_ASSERT(pid_format_base(0, (pid_t)(-(int64_t)PID_MAX_VAL - 1), 10) + 1 <= PID_FMT, "PID_FMT too small") ;

    char b[64] ; size_t n ;
    n = uid_format_base(b, (uid_t)0, 10) ;    b[n] = 0 ; TEST_ASSERT(!strcmp(b, "0"), "uid 0") ;
    n = uid_format_base(b, (uid_t)255, 16) ;  b[n] = 0 ; TEST_ASSERT(!strcmp(b, "ff"), "uid 255 hex") ;
    n = uid_format_base(b, (uid_t)4294967295U, 10) ; b[n] = 0 ; TEST_ASSERT(!strcmp(b, "4294967295"), "uid max") ;
    n = pid_format_base(b, (pid_t)-1, 10) ;   b[n] = 0 ; TEST_ASSERT(!strcmp(b, "-1"), "pid -1") ;
    n = pid_format_base(b, (pid_t)-2147483647 - 1, 10) ; b[n] = 0 ; TEST_ASSERT(!strcmp(b, "-2147483648"), "pid min") ;
    TEST_END() ;
}

/* ---- golden scan anchors (base 10), captured from skalibs parity ---- */

/* uid_t and gid_t share the same unsigned cap (UINT32 here). */
typedef struct ug_s { char const *s ; size_t pos ; uint32_t val ; } ug_t ;
typedef struct pp_s { char const *s ; size_t pos ; pid_t val ; } pp_t ;

static void test_scan(void)
{
    TEST_START("test_scan") ;

    static ug_t const ug[] = {
        { "", 0, 0 }, { " ", 0, 0 }, { "z", 0, 0 }, { "-1", 0, 0 }, { "+1", 0, 0 },
        { "-", 0, 0 }, { "ff", 0, 0 }, { "-ff", 0, 0 },
        { "0", 1, 0 }, { "255", 3, 255 }, { "65535", 5, 65535 },
        { "2147483647", 10, 2147483647U }, { "2147483648", 10, 2147483648U },
        { "4294967295", 10, 4294967295U },
        { "4294967296", 9, 429496729U },        /* > UINT32_MAX -> stops */
        { "5000000000", 9, 500000000U },
        { "9223372036854775807", 9, 922337203U },
        { "18446744073709551616", 10, 1844674407U },
        { "100000000", 9, 100000000U },
    } ;
    for (size_t i = 0 ; i < sizeof ug/sizeof*ug ; i++) {
        uid_t u = (uid_t)0xa5a5a5a5u ; gid_t g = (gid_t)0xa5a5a5a5u ;
        size_t pu = uid_parse_base(ug[i].s, &u, 10) ;
        size_t pg = gid_parse_base(ug[i].s, &g, 10) ;
        TEST_ASSERT(pu == ug[i].pos && pg == ug[i].pos, "uid/gid scan position") ;
        if (pu) TEST_ASSERT((uint32_t)u == ug[i].val && (uint32_t)g == ug[i].val, "uid/gid scan value") ;
    }

    static pp_t const pp[] = {
        { "", 0, 0 }, { " ", 0, 0 }, { "z", 0, 0 }, { "-", 0, 0 },
        { "ff", 0, 0 }, { "-ff", 0, 0 },
        { "0", 1, 0 }, { "-1", 2, -1 }, { "+1", 2, 1 }, { "255", 3, 255 },
        { "2147483647", 10, 2147483647 },
        { "2147483648", 9, 214748364 },         /* > INT32_MAX -> stops */
        { "-2147483648", 11, (pid_t)(-2147483647 - 1) },
        { "-2147483649", 10, -214748364 },
        { "4294967295", 9, 429496729 },
        { "100000000", 9, 100000000 },
    } ;
    for (size_t i = 0 ; i < sizeof pp/sizeof*pp ; i++) {
        pid_t p = (pid_t)0x5a5a5a5a ;
        size_t pq = pid_parse_base(pp[i].s, &p, 10) ;
        TEST_ASSERT(pq == pp[i].pos, "pid scan position") ;
        if (pq) TEST_ASSERT(p == pp[i].val, "pid scan value") ;
    }
    TEST_END() ;
}

int main(void)
{
    printf("=== uid/gid/pid conversion test suite ===\n") ;
    test_fmt() ;
    test_scan() ;
    test_roundtrip() ;
    printf("All types tests passed.\n") ;
    return 0 ;
}
