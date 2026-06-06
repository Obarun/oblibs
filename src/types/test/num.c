/*
 * num.c
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
 * Self-contained test for the num_* conversions (oblibs/types.h). No skalibs.
 * Two complementary strategies:
 *   - ROUND-TRIP over wide deterministic sweeps: u64_fmtbase then u64_scan_base
 *     (and the signed / narrow variants) must reproduce the original value and
 *     consume the whole string -- this proves fmt and scan are mutually
 *     consistent across every base without any external oracle.
 *   - GOLDEN anchors for absolute correctness: a handful of arithmetic facts
 *     (e.g. 255 base16 == "ff") plus the subtler scan rejection / overflow-stop
 *     and digit-mapping behaviours, whose exact values were CAPTURED from the
 *     original byte-exact differential run against skalibs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

/* deterministic generator (MMIX LCG) — no rand(), reproducible */
static uint64_t lcg_state = 0x123456789abcdef0ULL ;
static uint64_t lcg_next(void)
{
    lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL ;
    return lcg_state ;
}

/* ---- round-trip: fmt then scan reproduces the value and consumes it all ---- */

static void rt_u64(uint64_t x, uint8_t base)
{
    char buf[128] ;
    size_t len = u64_fmtbase(buf, x, base) ;
    buf[len] = 0 ;
    uint64_t v = ~x ;
    size_t p = u64_scan_base(buf, &v, base) ;
    TEST_ASSERT(p == len, "u64 round-trip did not consume the whole string") ;
    TEST_ASSERT(v == x, "u64 round-trip value mismatch") ;
}

static void rt_i64(int64_t d, uint8_t base)
{
    char buf[128] ;
    size_t len = i64_fmtbase(buf, d, base) ;
    buf[len] = 0 ;
    int64_t v = ~d ;
    size_t p = i64_scan_base(buf, &v, base) ;
    TEST_ASSERT(p == len, "i64 round-trip did not consume the whole string") ;
    TEST_ASSERT(v == d, "i64 round-trip value mismatch") ;
}

static void rt_narrow(uint64_t r, uint8_t base)
{
    char buf[128] ; size_t len ;
    uint32_t v32 = (uint32_t)r, o32 = ~v32 ;
    uint16_t v16 = (uint16_t)r, o16 = (uint16_t)~v16 ;
    uint8_t  v8  = (uint8_t)r,  o8  = (uint8_t)~v8 ;

    len = u64_fmtbase(buf, v32, base) ; buf[len] = 0 ;
    TEST_ASSERT(u32_scan_base(buf, &o32, base) == len && o32 == v32, "u32 round-trip") ;
    len = u64_fmtbase(buf, v16, base) ; buf[len] = 0 ;
    TEST_ASSERT(u16_scan_base(buf, &o16, base) == len && o16 == v16, "u16 round-trip") ;
    len = u64_fmtbase(buf, v8, base) ; buf[len] = 0 ;
    TEST_ASSERT(u8_scan_base(buf, &o8, base) == len && o8 == v8, "u8 round-trip") ;
}

static void test_roundtrip(void)
{
    TEST_START("test_roundtrip") ;
    for (uint64_t x = 0 ; x <= 100000 ; x++)
        for (uint8_t base = 2 ; base <= 36 ; base++)
            rt_u64(x, base) ;
    for (long i = 0 ; i < 60000 ; i++) {
        uint64_t x = lcg_next() ;
        int64_t d = (int64_t)x ;
        for (uint8_t base = 2 ; base <= 36 ; base++) {
            rt_u64(x, base) ;
            rt_i64(d, base) ;
            rt_narrow(x, base) ;
        }
    }
    /* signed edges */
    static int64_t const ie[] = { 0, 1, -1, INT64_MAX, INT64_MIN, INT64_MIN + 1,
                                  2147483647LL, -2147483648LL, 127, -128 } ;
    for (uint8_t base = 2 ; base <= 36 ; base++)
        for (size_t i = 0 ; i < sizeof ie/sizeof*ie ; i++)
            rt_i64(ie[i], base) ;
    /* unsigned edges */
    static uint64_t const ue[] = { 0, 1, 255, 256, 65535, 65536, 4294967295ULL,
                                   4294967296ULL, UINT64_MAX, UINT64_MAX - 1 } ;
    for (uint8_t base = 2 ; base <= 36 ; base++)
        for (size_t i = 0 ; i < sizeof ue/sizeof*ue ; i++)
            rt_u64(ue[i], base) ;
    TEST_END() ;
}

/* ---- golden fmt anchors (arithmetic facts, base-independent of any oracle) ---- */

static void fmt_eq(uint64_t x, uint8_t base, char const *exp)
{
    char buf[128] ;
    size_t len = u64_fmtbase(buf, x, base) ;
    buf[len] = 0 ;
    TEST_ASSERT(len == strlen(exp) && !strcmp(buf, exp), "u64_fmtbase golden mismatch") ;
    TEST_ASSERT(u64_fmtbase(0, x, base) == len, "u64_fmtbase length-only mismatch") ;
}
static void ifmt_eq(int64_t d, uint8_t base, char const *exp)
{
    char buf[128] ;
    size_t len = i64_fmtbase(buf, d, base) ;
    buf[len] = 0 ;
    TEST_ASSERT(len == strlen(exp) && !strcmp(buf, exp), "i64_fmtbase golden mismatch") ;
}
static void pad_eq(uint64_t x, size_t n, uint8_t base, char const *exp)
{
    char buf[128] ;
    size_t len = u64_fmtpad(buf, x, n, base) ;
    buf[len] = 0 ;
    TEST_ASSERT(len == strlen(exp) && !strcmp(buf, exp), "u64_fmtpad golden mismatch") ;
}

static void test_fmt_golden(void)
{
    TEST_START("test_fmt_golden") ;
    fmt_eq(0, 10, "0") ; fmt_eq(0, 16, "0") ; fmt_eq(0, 2, "0") ;
    fmt_eq(255, 10, "255") ; fmt_eq(255, 16, "ff") ; fmt_eq(255, 8, "377") ;
    fmt_eq(255, 2, "11111111") ;
    fmt_eq(256, 16, "100") ; fmt_eq(10, 2, "1010") ;
    fmt_eq(4294967295ULL, 16, "ffffffff") ; fmt_eq(4294967295ULL, 10, "4294967295") ;
    fmt_eq(UINT64_MAX, 16, "ffffffffffffffff") ;
    fmt_eq(UINT64_MAX, 10, "18446744073709551615") ;
    fmt_eq(35, 36, "z") ; fmt_eq(36, 36, "10") ;

    ifmt_eq(0, 10, "0") ; ifmt_eq(-1, 10, "-1") ; ifmt_eq(-128, 10, "-128") ;
    ifmt_eq(255, 16, "ff") ; ifmt_eq(-255, 16, "-ff") ;
    ifmt_eq(INT64_MIN, 10, "-9223372036854775808") ;
    ifmt_eq(INT64_MAX, 10, "9223372036854775807") ;

    pad_eq(5, 3, 10, "005") ; pad_eq(255, 4, 16, "00ff") ; pad_eq(0, 1, 10, "0") ;
    pad_eq(123, 2, 10, "123") ; pad_eq(0, 4, 2, "0000") ;
    TEST_END() ;
}

/* ---- golden scan anchors: rejection (p==0) and overflow-stop semantics ---- */

typedef struct uscan_s { char const *s ; uint8_t base ; size_t pos ; uint64_t val ; } uscan_t ;

static void test_scan_golden(void)
{
    TEST_START("test_scan_golden") ;
    /* pos==0 means rejected: value is left untouched (not checked). */
    static uscan_t const g[] = {
        { "", 10, 0, 0 }, { " ", 10, 0, 0 }, { "z", 10, 0, 0 },
        { "-1", 10, 0, 0 }, { "+1", 10, 0, 0 }, { "ff", 10, 0, 0 },
        { "00000", 10, 5, 0 },
        { "ff", 16, 2, 255 }, { "FF", 16, 2, 255 },
        { "18446744073709551615", 10, 20, 18446744073709551615ULL },
        { "18446744073709551616", 10, 19, 1844674407370955161ULL },   /* overflow stops */
        { "ffffffffffffffff", 16, 16, 18446744073709551615ULL },
        { "10000000000000000", 10, 17, 10000000000000000ULL },
        { "12345 trail", 10, 5, 12345 },
        { "0777stop", 10, 4, 777 }, { "0777stop", 16, 4, 1911 },
    } ;
    for (size_t i = 0 ; i < sizeof g/sizeof*g ; i++) {
        uint64_t v = 0xdeadbeefcafebabeULL ;
        size_t p = u64_scan_base(g[i].s, &v, g[i].base) ;
        TEST_ASSERT(p == g[i].pos, "u64 scan golden position") ;
        if (p) TEST_ASSERT(v == g[i].val, "u64 scan golden value") ;
    }

    /* narrow widths stop before exceeding their type max (capture from skalibs). */
    struct { char const *s ; size_t p ; uint32_t v ; } const n32[] = {
        { "4294967295", 10, 4294967295U }, { "4294967296", 9, 429496729U }, { "256", 3, 256 } } ;
    for (size_t i = 0 ; i < sizeof n32/sizeof*n32 ; i++) {
        uint32_t v = 0 ; size_t p = u32_scan_base(n32[i].s, &v, 10) ;
        TEST_ASSERT(p == n32[i].p && v == n32[i].v, "u32 scan cap golden") ;
    }
    struct { char const *s ; size_t p ; uint16_t v ; } const n16[] = {
        { "65535", 5, 65535 }, { "65536", 4, 6553 }, { "256", 3, 256 } } ;
    for (size_t i = 0 ; i < sizeof n16/sizeof*n16 ; i++) {
        uint16_t v = 0 ; size_t p = u16_scan_base(n16[i].s, &v, 10) ;
        TEST_ASSERT(p == n16[i].p && v == n16[i].v, "u16 scan cap golden") ;
    }
    struct { char const *s ; size_t p ; uint8_t v ; } const n8[] = {
        { "255", 3, 255 }, { "256", 2, 25 }, { "257", 2, 25 } } ;
    for (size_t i = 0 ; i < sizeof n8/sizeof*n8 ; i++) {
        uint8_t v = 0 ; size_t p = u8_scan_base(n8[i].s, &v, 10) ;
        TEST_ASSERT(p == n8[i].p && v == n8[i].v, "u8 scan cap golden") ;
    }
    TEST_END() ;
}

typedef struct iscan_s { char const *s ; size_t pos ; int64_t val ; } iscan_t ;

static void test_iscan_golden(void)
{
    TEST_START("test_iscan_golden") ;
    static iscan_t const g[] = {
        { "", 0, 0 }, { "z", 0, 0 }, { "-", 0, 0 }, { "+", 0, 0 },
        { "0", 1, 0 }, { "-1", 2, -1 }, { "+1", 2, 1 }, { "127", 3, 127 }, { "-128", 4, -128 },
        { "9223372036854775807", 19, INT64_MAX },
        { "-9223372036854775808", 20, INT64_MIN },
        { "9223372036854775808", 18, 922337203685477580LL },    /* overflow stops */
        { "-9223372036854775809", 19, -922337203685477580LL },
        { "-123 trail", 4, -123 },
    } ;
    for (size_t i = 0 ; i < sizeof g/sizeof*g ; i++) {
        int64_t v = 0x5a5a5a5a5a5a5a5aLL ;
        size_t p = i64_scan_base(g[i].s, &v, 10) ;
        TEST_ASSERT(p == g[i].pos, "i64 scan golden position") ;
        if (p) TEST_ASSERT(v == g[i].val, "i64 scan golden value") ;
    }
    TEST_END() ;
}

/* ---- golden digit mapping (captured from skalibs fmtscan_asc / fmtscan_num) ---- */

static void test_digit_golden(void)
{
    TEST_START("test_digit_golden") ;
    static char const *const map = "0123456789abcdefghijklmnopqrstuvwxyz" ;
    for (int v = 0 ; v <= 35 ; v++)
        TEST_ASSERT(fmt_digit((unsigned char)v) == map[v], "fmt_digit 0..35") ;
    /* fmt_digit of an out-of-range value is never requested by callers, so its
     * return is left unspecified and not asserted here. */

    /* scan_digit returns the digit value, or >= base when @c is not a digit of
     * that base (callers test "< base"); non-alphanumerics yield exactly base. */
    struct { char c ; uint8_t base ; unsigned exp ; } const sd[] = {
        { '0', 2, 0 }, { '0', 10, 0 }, { '0', 16, 0 }, { '0', 36, 0 },
        { '9', 2, 2 }, { '9', 10, 9 }, { '9', 16, 9 }, { '9', 36, 9 },
        { 'a', 2, 2 }, { 'a', 10, 10 }, { 'a', 16, 10 }, { 'a', 36, 10 },
        { 'f', 16, 15 }, { 'f', 36, 15 },
        { 'g', 16, 16 }, { 'g', 36, 16 },
        { 'z', 16, 16 }, { 'z', 36, 35 },
        { 'A', 16, 10 }, { 'Z', 36, 35 },
        { ':', 16, 16 }, { ':', 36, 36 },
        { '/', 16, 16 }, { '/', 36, 36 },
        { ' ', 16, 16 }, { ' ', 36, 36 },
    } ;
    for (size_t i = 0 ; i < sizeof sd/sizeof*sd ; i++)
        TEST_ASSERT(scan_digit((unsigned char)sd[i].c, sd[i].base) == sd[i].exp, "scan_digit golden") ;
    TEST_END() ;
}

/* buffer-size constants must hold the worst case + null terminator. */
static void test_sizes(void)
{
    TEST_START("test_sizes") ;
#define CK_U(MAX, F, O, X, B) do { \
        TEST_ASSERT(u64_fmtbase(0,(MAX),10) + 1 <= (F), "U" #F " too small (dec)") ; \
        TEST_ASSERT(u64_fmtbase(0,(MAX),8)  + 1 <= (O), "U" #O " too small (oct)") ; \
        TEST_ASSERT(u64_fmtbase(0,(MAX),16) + 1 <= (X), "U" #X " too small (hex)") ; \
        TEST_ASSERT(u64_fmtbase(0,(MAX),2)  + 1 <= (B), "U" #B " too small (bin)") ; \
    } while(0)
    CK_U(UINT8_MAX,  U8_FMT,  U8_OFMT,  U8_XFMT,  U8_BFMT) ;
    CK_U(UINT16_MAX, U16_FMT, U16_OFMT, U16_XFMT, U16_BFMT) ;
    CK_U(UINT32_MAX, U32_FMT, U32_OFMT, U32_XFMT, U32_BFMT) ;
    CK_U(UINT64_MAX, U64_FMT, U64_OFMT, U64_XFMT, U64_BFMT) ;
#undef CK_U
#define CK_I(MIN, F, O, X, B) do { \
        TEST_ASSERT(i64_fmtbase(0,(MIN),10) + 1 <= (F), "I" #F " too small (dec)") ; \
        TEST_ASSERT(i64_fmtbase(0,(MIN),8)  + 1 <= (O), "I" #O " too small (oct)") ; \
        TEST_ASSERT(i64_fmtbase(0,(MIN),16) + 1 <= (X), "I" #X " too small (hex)") ; \
        TEST_ASSERT(i64_fmtbase(0,(MIN),2)  + 1 <= (B), "I" #B " too small (bin)") ; \
    } while(0)
    CK_I(INT8_MIN,  I8_FMT,  I8_OFMT,  I8_XFMT,  I8_BFMT) ;
    CK_I(INT16_MIN, I16_FMT, I16_OFMT, I16_XFMT, I16_BFMT) ;
    CK_I(INT32_MIN, I32_FMT, I32_OFMT, I32_XFMT, I32_BFMT) ;
    CK_I(INT64_MIN, I64_FMT, I64_OFMT, I64_XFMT, I64_BFMT) ;
#undef CK_I
    TEST_END() ;
}

/* the width macros are thin wrappers over the (already round-tripped) engine;
   check they expand correctly and produce the same bytes as a direct call. */
static void test_macros(void)
{
    TEST_START("test_macros") ;
    char m[128], e[128] ;
    size_t lm, le ;
#define CK_UMAC(MAC, V, BASE) do { \
        lm = MAC(m, (V)) ; le = u64_fmtbase(e, (uint64_t)(V), (BASE)) ; \
        TEST_ASSERT(lm == le && !memcmp(m, e, lm), #MAC " wrong") ; \
    } while(0)
    CK_UMAC(u8_fmt, 200, 10) ;  CK_UMAC(u8_ofmt, 200, 8) ;
    CK_UMAC(u8_xfmt, 200, 16) ; CK_UMAC(u8_bfmt, 200, 2) ;
    CK_UMAC(u16_fmt, 65000, 10) ; CK_UMAC(u16_xfmt, 65000, 16) ;
    CK_UMAC(u32_fmt, 4000000000U, 10) ; CK_UMAC(u32_xfmt, 4000000000U, 16) ;
    CK_UMAC(u64_fmt, 12345678901234ULL, 10) ; CK_UMAC(u64_bfmt, 12345678901234ULL, 2) ;
#undef CK_UMAC
#define CK_IMAC(MAC, V, BASE) do { \
        lm = MAC(m, (V)) ; le = i64_fmtbase(e, (int64_t)(V), (BASE)) ; \
        TEST_ASSERT(lm == le && !memcmp(m, e, lm), #MAC " wrong") ; \
    } while(0)
    CK_IMAC(i8_fmt, -120, 10) ;  CK_IMAC(i8_ofmt, -120, 8) ;
    CK_IMAC(i8_xfmt, -120, 16) ; CK_IMAC(i8_bfmt, -120, 2) ;
    CK_IMAC(i16_fmt, -30000, 10) ; CK_IMAC(i32_fmt, -2000000000, 10) ;
    CK_IMAC(i64_fmt, -9000000000000LL, 10) ; CK_IMAC(i64_xfmt, -9000000000000LL, 16) ;
#undef CK_IMAC
    TEST_END() ;
}

int main(void)
{
    printf("=== num test suite ===\n") ;
    test_digit_golden() ;
    test_sizes() ;
    test_macros() ;
    test_fmt_golden() ;
    test_scan_golden() ;
    test_iscan_golden() ;
    test_roundtrip() ;
    printf("All num tests passed.\n") ;
    return 0 ;
}
