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
 * Differential test: every num_* function is compared byte-for-byte against the
 * skalibs function it replaces. Any mismatch fails the test. This is the proof
 * that the replacement introduces no regression vs skalibs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <oblibs/types.h>

#include <skalibs/uint64.h>   /* uint64_fmt_generic, uint640_fmt_generic, int64_fmt_generic, uint64_scan_base[_max] */
#include <skalibs/uint32.h>   /* uint32_scan_base */
#include <skalibs/uint16.h>   /* uint16_scan_base */
#include <skalibs/fmtscan.h>  /* fmtscan_asc, fmtscan_num */

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

/* compare u64_fmtbase vs uint64_fmt_generic for one (x, base) */
static void check_fmt(uint64_t x, uint8_t base)
{
    char a[128], b[128] ;
    size_t la = u64_fmtbase(a, x, base) ;
    size_t lb = uint64_fmt_generic(b, x, base) ;
    TEST_ASSERT(la == lb, "u64_fmtbase length differs from skalibs") ;
    TEST_ASSERT(!memcmp(a, b, la), "u64_fmtbase bytes differ from skalibs") ;
    /* length-only mode (s == 0) */
    TEST_ASSERT(u64_fmtbase(0, x, base) == uint64_fmt_generic(0, x, base),
                "u64_fmtbase length-only differs from skalibs") ;
}

/* compare u64_fmtpad vs uint640_fmt_generic for one (x, n, base) */
static void check_fmtpad(uint64_t x, size_t n, uint8_t base)
{
    char a[128], b[128] ;
    size_t la = u64_fmtpad(a, x, n, base) ;
    size_t lb = uint640_fmt_generic(b, x, n, base) ;
    TEST_ASSERT(la == lb, "u64_fmtpad length differs from skalibs") ;
    TEST_ASSERT(!memcmp(a, b, la), "u64_fmtpad bytes differ from skalibs") ;
    TEST_ASSERT(u64_fmtpad(0, x, n, base) == uint640_fmt_generic(0, x, n, base),
                "u64_fmtpad length-only differs from skalibs") ;
}

/* compare i64_fmtbase vs int64_fmt_generic for one (d, base) */
static void check_ifmt(int64_t d, uint8_t base)
{
    char a[128], b[128] ;
    size_t la = i64_fmtbase(a, d, base) ;
    size_t lb = int64_fmt_generic(b, d, base) ;
    TEST_ASSERT(la == lb, "i64_fmtbase length differs from skalibs") ;
    TEST_ASSERT(!memcmp(a, b, la), "i64_fmtbase bytes differ from skalibs") ;
    TEST_ASSERT(i64_fmtbase(0, d, base) == int64_fmt_generic(0, d, base),
                "i64_fmtbase length-only differs from skalibs") ;
}

/* compare u64_scan_base vs uint64_scan_base for one (s, base) */
static void check_scan(char const *s, uint8_t base)
{
    uint64_t va = 0xa5a5a5a5a5a5a5a5ULL, vb = 0xa5a5a5a5a5a5a5a5ULL ;  /* same sentinel */
    size_t pa = u64_scan_base(s, &va, base) ;
    size_t pb = uint64_scan_base(s, &vb, base) ;
    TEST_ASSERT(pa == pb, "u64_scan_base position differs from skalibs") ;
    TEST_ASSERT(va == vb, "u64_scan_base value differs from skalibs") ;
}

static void test_fmt_edge(void)
{
    TEST_START("test_fmt_edge") ;
    static uint64_t const edges[] = {
        0, 1, 2, 7, 8, 9, 10, 15, 16, 31, 32, 63, 64, 99, 100, 127, 128,
        255, 256, 999, 1000, 65535, 65536, 1000000, 4294967295ULL, 4294967296ULL,
        9999999999ULL, 10000000000ULL, 18446744073709551614ULL, UINT64_MAX
    } ;
    for (uint8_t base = 2 ; base <= 36 ; base++) {
        /* powers of base around length transitions */
        uint64_t p = 1 ;
        for (int k = 0 ; k < 13 ; k++) {
            check_fmt(p - 1, base) ; check_fmt(p, base) ; check_fmt(p + 1, base) ;
            if (p > UINT64_MAX / base) break ;
            p *= base ;
        }
        for (size_t i = 0 ; i < sizeof(edges)/sizeof(*edges) ; i++)
            check_fmt(edges[i], base) ;
    }
    TEST_END() ;
}

static void test_fmt_sweep(void)
{
    TEST_START("test_fmt_sweep") ;
    /* exhaustive small range */
    for (uint64_t x = 0 ; x <= 200000 ; x++)
        for (uint8_t base = 2 ; base <= 36 ; base++)
            check_fmt(x, base) ;
    /* random 64-bit sweep */
    for (long i = 0 ; i < 100000 ; i++) {
        uint64_t x = lcg_next() ;
        for (uint8_t base = 2 ; base <= 36 ; base++)
            check_fmt(x, base) ;
    }
    TEST_END() ;
}

static void test_fmtpad(void)
{
    TEST_START("test_fmtpad") ;
    static uint64_t const vals[] = { 0, 1, 9, 10, 99, 100, 65535, 4294967295ULL, UINT64_MAX } ;
    for (uint8_t base = 2 ; base <= 36 ; base++)
        for (size_t n = 0 ; n <= 40 ; n++)
            for (size_t i = 0 ; i < sizeof(vals)/sizeof(*vals) ; i++)
                check_fmtpad(vals[i], n, base) ;
    /* random */
    for (long i = 0 ; i < 50000 ; i++) {
        uint64_t x = lcg_next() ;
        size_t n = lcg_next() % 41 ;
        uint8_t base = 2 + lcg_next() % 35 ;
        check_fmtpad(x, n, base) ;
    }
    TEST_END() ;
}

static void test_ifmt(void)
{
    TEST_START("test_ifmt") ;
    static int64_t const edges[] = {
        0, 1, -1, 9, -9, 10, -10, 127, -128, 255, -256,
        2147483647LL, -2147483648LL, INT64_MAX, INT64_MIN, INT64_MIN + 1
    } ;
    for (uint8_t base = 2 ; base <= 36 ; base++)
        for (size_t i = 0 ; i < sizeof(edges)/sizeof(*edges) ; i++)
            check_ifmt(edges[i], base) ;
    for (long i = 0 ; i < 100000 ; i++) {
        int64_t d = (int64_t)lcg_next() ;
        for (uint8_t base = 2 ; base <= 36 ; base++)
            check_ifmt(d, base) ;
    }
    TEST_END() ;
}

/* narrow-width scans vs skalibs */
static void check_scan32(char const *s, uint8_t base)
{
    uint32_t a = 0xa5a5a5a5u, b = 0xa5a5a5a5u ;  /* same sentinel */
    size_t pa = u32_scan_base(s, &a, base) ;
    size_t pb = uint32_scan_base(s, &b, base) ;
    TEST_ASSERT(pa == pb, "u32_scan_base position differs from skalibs") ;
    TEST_ASSERT(a == b, "u32_scan_base value differs from skalibs") ;
}

static void check_scan16(char const *s, uint8_t base)
{
    uint16_t a = 0xa5a5u, b = 0xa5a5u ;
    size_t pa = u16_scan_base(s, &a, base) ;
    size_t pb = uint16_scan_base(s, &b, base) ;
    TEST_ASSERT(pa == pb, "u16_scan_base position differs from skalibs") ;
    TEST_ASSERT(a == b, "u16_scan_base value differs from skalibs") ;
}

/* skalibs has no uint8 scan: reference is the 64-bit engine capped at UINT8_MAX */
static void check_scan8(char const *s, uint8_t base)
{
    uint8_t a = 0xa5 ;
    uint64_t ref = 0 ;
    size_t pa = u8_scan_base(s, &a, base) ;
    size_t pb = uint64_scan_base_max(s, &ref, base, UINT8_MAX) ;
    TEST_ASSERT(pa == pb, "u8_scan_base position differs from skalibs (capped)") ;
    if (pa) TEST_ASSERT(a == (uint8_t)ref, "u8_scan_base value differs from skalibs (capped)") ;
}

static void test_scan_narrow(void)
{
    TEST_START("test_scan_narrow") ;
    /* round-trip: values within each width's range, formatted then scanned back */
    for (long i = 0 ; i < 50000 ; i++) {
        uint64_t r = lcg_next() ;
        uint32_t v32 = (uint32_t)r ;
        uint16_t v16 = (uint16_t)r ;
        uint8_t  v8  = (uint8_t)r ;
        for (uint8_t base = 2 ; base <= 36 ; base++) {
            char buf[128] ; size_t len ;
            len = u64_fmtbase(buf, v32, base) ; buf[len] = 0 ; check_scan32(buf, base) ;
            len = u64_fmtbase(buf, v16, base) ; buf[len] = 0 ; check_scan16(buf, base) ;
            len = u64_fmtbase(buf, v8,  base) ; buf[len] = 0 ; check_scan8(buf, base) ;
        }
    }
    /* overflow / boundary / invalid strings — each width must cap (or stop) like skalibs */
    static char const *str[] = {
        "", " ", "z", "-1",
        "254", "255", "256", "257",
        "65534", "65535", "65536", "65537",
        "4294967294", "4294967295", "4294967296", "4294967297",
        "999999999999", "18446744073709551615", "18446744073709551616",
        " fe", "fe", "ff", "100", "fff", "ffff", "10000", "ffffffff", "100000000",
        "00000000255", "777", "11111111"
    } ;
    for (size_t i = 0 ; i < sizeof(str)/sizeof(*str) ; i++)
        for (uint8_t base = 2 ; base <= 36 ; base++) {
            check_scan32(str[i], base) ;
            check_scan16(str[i], base) ;
            check_scan8(str[i], base) ;
        }
    TEST_END() ;
}

/* signed scan engine vs skalibs */
static void check_iscan(char const *s, uint8_t base)
{
    int64_t a = 0x5a5a5a5a5a5a5a5aLL, b = 0x5a5a5a5a5a5a5a5aLL ;  /* same sentinel */
    size_t pa = i64_scan_base(s, &a, base) ;
    size_t pb = int64_scan_base(s, &b, base) ;
    TEST_ASSERT(pa == pb, "i64_scan_base position differs from skalibs") ;
    TEST_ASSERT(a == b, "i64_scan_base value differs from skalibs") ;
}

static void check_iscan_max(char const *s, uint8_t base, uint64_t max)
{
    int64_t a = 0x5a5a5a5a5a5a5a5aLL, b = 0x5a5a5a5a5a5a5a5aLL ;
    size_t pa = i64_scan_base_max(s, &a, base, max) ;
    size_t pb = int64_scan_base_max(s, &b, base, max) ;
    TEST_ASSERT(pa == pb, "i64_scan_base_max position differs from skalibs") ;
    TEST_ASSERT(a == b, "i64_scan_base_max value differs from skalibs") ;
}

static void test_iscan(void)
{
    TEST_START("test_iscan") ;
    static char const *str[] = {
        "", " ", "z", "-", "+", "--1", "+-1",
        "0", "-0", "1", "-1", "+1", "127", "-128",
        "2147483647", "-2147483648", "+2147483647",
        "9223372036854775807", "-9223372036854775808",
        "9223372036854775808", "-9223372036854775809",   /* overflow */
        "18446744073709551616", "-99999999999999999999999",
        "-7fff", "ff", "-ff", "  -5", "-123 trail"
    } ;
    static uint64_t const maxes[] = { 127, 255, INT16_MAX, INT32_MAX, UINT32_MAX, INT64_MAX } ;
    for (uint8_t base = 2 ; base <= 36 ; base++) {
        for (size_t i = 0 ; i < sizeof(str)/sizeof(*str) ; i++) {
            check_iscan(str[i], base) ;
            for (size_t m = 0 ; m < sizeof(maxes)/sizeof(*maxes) ; m++)
                check_iscan_max(str[i], base, maxes[m]) ;
        }
    }
    /* round-trip via i64_fmtbase + random/edge values */
    static int64_t const edges[] = { 0, 1, -1, INT64_MAX, INT64_MIN, INT64_MIN + 1,
                                     2147483647LL, -2147483648LL } ;
    for (uint8_t base = 2 ; base <= 36 ; base++)
        for (size_t i = 0 ; i < sizeof(edges)/sizeof(*edges) ; i++) {
            char buf[128] ; size_t len = i64_fmtbase(buf, edges[i], base) ; buf[len] = 0 ;
            check_iscan(buf, base) ;
        }
    for (long i = 0 ; i < 100000 ; i++) {
        int64_t d = (int64_t)lcg_next() ;
        for (uint8_t base = 2 ; base <= 36 ; base++) {
            char buf[128] ; size_t len = i64_fmtbase(buf, d, base) ; buf[len] = 0 ;
            check_iscan(buf, base) ;
        }
    }
    TEST_END() ;
}

static void test_scan(void)
{
    TEST_START("test_scan") ;
    /* round-trip: format a value, then scan it back, compared against skalibs */
    for (long i = 0 ; i < 100000 ; i++) {
        uint64_t x = lcg_next() ;
        for (uint8_t base = 2 ; base <= 36 ; base++) {
            char buf[128] ;
            size_t len = u64_fmtbase(buf, x, base) ;
            buf[len] = 0 ;
            check_scan(buf, base) ;
        }
    }
    /* invalid / boundary inputs */
    static char const *bad[] = {
        "", " ", "z", "/", ":", "-1", "+1", "  10", "\0",
        "00000", "0x10", "ff", "FF", "9a", "abcXYZ",
        "18446744073709551615",   /* UINT64_MAX in base 10 */
        "18446744073709551616",   /* overflow base 10 */
        "99999999999999999999999999",  /* huge overflow */
        "ffffffffffffffff",       /* UINT64_MAX in base 16 */
        "10000000000000000",      /* overflow base 16 */
        "12345678 trailing", "0777stop"
    } ;
    for (size_t i = 0 ; i < sizeof(bad)/sizeof(*bad) ; i++)
        for (uint8_t base = 2 ; base <= 36 ; base++)
            check_scan(bad[i], base) ;
    TEST_END() ;
}

static void test_digit(void)
{
    TEST_START("test_digit") ;
    for (int v = 0 ; v <= 255 ; v++)
        TEST_ASSERT(fmt_digit((unsigned char)v) == (char)fmtscan_asc((unsigned char)v),
                    "fmt_digit differs from fmtscan_asc") ;
    for (int c = 0 ; c <= 255 ; c++)
        for (int base = 0 ; base <= 36 ; base++)
            TEST_ASSERT(scan_digit((unsigned char)c, (unsigned char)base)
                        == fmtscan_num((unsigned char)c, (unsigned char)base),
                        "scan_digit differs from fmtscan_num") ;
    TEST_END() ;
}

/* buffer-size constants must hold the worst case + null terminator:
   len(max value) + 1 <= declared size. Proves no overflow in callers. */
static void test_sizes(void)
{
    TEST_START("test_sizes") ;
    /* unsigned: widest value is the type max */
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
    /* signed: widest representation is the type min (leading '-') */
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

/* the width macros are thin wrappers over the (already proven) engine; check
   they expand correctly and produce the same bytes as a direct engine call. */
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
    printf("=== num differential test vs skalibs ===\n") ;
    test_digit() ;
    test_sizes() ;
    test_macros() ;
    test_fmt_edge() ;
    test_fmtpad() ;
    test_ifmt() ;
    test_scan() ;
    test_scan_narrow() ;
    test_iscan() ;
    test_fmt_sweep() ;
    printf("All num tests passed.\n") ;
    return 0 ;
}
