/*
 * environ_untrim_value.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

/* Tests for the envfile VALUE ENCODER:
 *   - environ_untrim_value(out, value)   -> appends envfile-syntax encoding
 *   - environ_untrim_kv(out, k, v)   -> appends "key=<encoded value>\n"
 *
 * Contract (TOTAL round-trip): a value containing a literal newline (0x0a) is
 * REJECTED by environ_untrim_value with errno EINVAL and nothing is appended to
 * `out` (0x0a is the module's entry separator and could not survive the whole
 * decode path). For every value the encoder ACCEPTS (i.e. every newline-free
 * value), decode(encode(v)) == v holds byte-for-byte through BOTH decode paths:
 *
 *     environ_trim     ( "KEY=" + environ_untrim_value(v) )  ==  v
 *     environ_get_value( environ_trim( "KEY=" + encode(v) ) ) ==  v
 *
 * environ_trim emits NUL-separated entries; for a single "KEY=..." entry the
 * value bytes start at parsed.s + 4 ("KEY=" is 4 bytes) and have length
 * parsed.len - 1 - 4 (parsed.len counts the entry's terminating NUL).
 *
 * The newline rejection makes the guarantee total: because every accepted value
 * is newline-free, the environ_get_value oracle (which uses 0x0a as separator)
 * now applies to EVERY accepted value, not just a subset. The old "newline
 * round-trips via trim but not get_value" caveat is gone with the rejection.
 *
 * Note: the `case '\n'` arm of environ_untrim_value's escape switch is now DEAD
 * code (0x0a is rejected up front and never reaches the switch); this suite
 * exercises the rejection path instead, see test_newline_rejected.
 *
 * Expected outputs were derived from the documented contract and cross-checked
 * against the real environ_trim implementation. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <oblibs/environ.h>
#include <oblibs/strbuf.h>

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

/* ------------------------------------------------------------------ helpers */

/* Encode @value as the value part of "KEY=..." into @enc (cleared first).
 * Returns the encoded bytes (the part AFTER "KEY="), NUL-terminated. @value
 * MUST be accepted by the encoder (newline-free). */
static char const *encode_value(strbuf *enc, char const *value)
{
    enc->len = 0 ;
    TEST_ASSERT(strbuf_cats(enc, "KEY="), "append KEY=") ;
    TEST_ASSERT(environ_untrim_value(enc, value), "environ_untrim_value") ;
    TEST_ASSERT(strbuf_uncounted(enc), "uncounted") ;
    return enc->s + 4 ;
}

/* THE trim oracle. Encode @value, decode with environ_trim, and compare the
 * decoded value bytes (entry bytes after "KEY=", up to the entry NUL) against
 * the original @value of length @vlen, byte for byte. */
static void roundtrip_trim(char const *value, size_t vlen, char const *msg)
{
    _alloc_strbuf_(enc, 64) ;
    enc.len = 0 ;
    TEST_ASSERT(strbuf_cats(&enc, "KEY="), "append KEY=") ;
    TEST_ASSERT(environ_untrim_value(&enc, value), "environ_untrim_value") ;
    TEST_ASSERT(strbuf_uncounted(&enc), "uncounted") ;

    _alloc_strbuf_(dec, 64) ;
    TEST_ASSERT(environ_trim(&dec, enc.s), msg) ;

    /* single entry expected: "KEY=<value>\0"  ->  dec.len == 4 + vlen + 1 */
    TEST_ASSERT(dec.len == 4 + vlen + 1, "decoded single-entry length") ;
    TEST_ASSERT(dec.len >= 5, "at least KEY= and a NUL") ;
    TEST_ASSERT(dec.s[0] == 'K' && dec.s[1] == 'E' && dec.s[2] == 'Y'
                && dec.s[3] == '=', "key preserved") ;
    char const *got = dec.s + 4 ;
    size_t glen = dec.len - 1 - 4 ;            /* value bytes (excl. entry NUL) */
    TEST_ASSERT(glen == vlen, "decoded value length matches") ;
    TEST_ASSERT(memcmp(got, value, vlen) == 0, msg) ;
    TEST_ASSERT(dec.s[dec.len - 1] == '\0', "entry NUL terminator") ;
}

/* The environ_get_value oracle. Since the encoder rejects newline-bearing
 * values, every value reaching here is newline-free and this oracle applies to
 * EVERY accepted value. Asserts environ_get_value extracts @value verbatim from
 * the encoded, trim-decoded "KEY=..." entry. */
static void roundtrip_get_value(char const *value, char const *msg)
{
    _alloc_strbuf_(enc, 64) ;
    enc.len = 0 ;
    TEST_ASSERT(strbuf_cats(&enc, "KEY="), "append KEY=") ;
    TEST_ASSERT(environ_untrim_value(&enc, value), "environ_untrim_value") ;
    TEST_ASSERT(strbuf_uncounted(&enc), "uncounted") ;

    _alloc_strbuf_(dec, 64) ;
    TEST_ASSERT(environ_trim(&dec, enc.s), "trim for get_value") ;

    _alloc_strbuf_(val, 64) ;
    TEST_ASSERT(environ_get_value(&val, dec.s), "environ_get_value") ;
    TEST_ASSERT(strbuf_uncounted(&val), "uncounted val") ;
    TEST_ASSERT(strlen(value) == val.len, "get_value length") ;
    TEST_ASSERT(memcmp(val.s, value, val.len) == 0, msg) ;
}

/* True iff the encoded form of @value (the byte right after "KEY=") is a '"'. */
static int encoded_is_quoted(char const *value)
{
    _alloc_strbuf_(enc, 64) ;
    char const *v = encode_value(&enc, value) ;
    return v[0] == '"' ;
}

/* Full battery on one ACCEPTED (newline-free) value: trim round-trip AND
 * get_value round-trip (the contract is now total for accepted values). */
static void rt(char const *value, char const *msg)
{
    roundtrip_trim(value, strlen(value), msg) ;
    roundtrip_get_value(value, msg) ;
}

/* Assert that environ_untrim_value REJECTS @value: returns 0, sets errno EINVAL,
 * and appends NOTHING to `out` (verified with a "KEY=" sentinel whose length
 * must not grow). */
static void assert_rejected(char const *value, char const *msg)
{
    _alloc_strbuf_(enc, 64) ;
    enc.len = 0 ;
    TEST_ASSERT(strbuf_cats(&enc, "KEY="), "append KEY= sentinel") ;
    size_t before = enc.len ;
    errno = 0 ;
    int r = environ_untrim_value(&enc, value) ;
    TEST_ASSERT(r == 0, msg) ;
    TEST_ASSERT(errno == EINVAL, "rejection sets errno EINVAL") ;
    TEST_ASSERT(enc.len == before, "rejection appends nothing to out") ;
    /* sentinel bytes untouched */
    TEST_ASSERT(enc.len == 4 && !memcmp(enc.s, "KEY=", 4),
                "out still holds only the KEY= sentinel") ;
}

/* ------------------------------------------------------------------- tests */

/* Clean values stay UNQUOTED: the first encoded byte is NOT '"', and they
 * round-trip on both decode paths. Covers the RAW path of environ_untrim_value
 * (value_needs_quote==0, strbuf_cats verbatim) plus the "literal #, =, internal
 * space, UTF-8" rules. */
static void test_clean_unquoted(void)
{
    TEST_START("test_clean_unquoted") ;
    char const *clean[] = {
        "tty1", "seat0", ":0", "1000", "/usr/bin:/sbin",
        "hello world",          /* internal space stays literal & unquoted */
        "a=b=c",                /* '=' literal in a value */
        "a#b", "#foo",          /* '#' literal in a value */
        "!2",                   /* unexport marker is just a byte to the parser */
        "caf\xc3\xa9",          /* UTF-8 "café": high bytes stay literal */
    } ;
    for (size_t i = 0 ; i < sizeof(clean)/sizeof(*clean) ; i++) {
        TEST_ASSERT(!encoded_is_quoted(clean[i]), clean[i]) ;
        rt(clean[i], clean[i]) ;
    }
    TEST_END() ;
}

/* Empty value: environ_untrim_value appends NOTHING (-> "KEY=") and decodes back
 * to "". Covers the value_needs_quote(empty)==0 early branch. */
static void test_empty(void)
{
    TEST_START("test_empty") ;
    _alloc_strbuf_(enc, 16) ;
    enc.len = 0 ;
    TEST_ASSERT(strbuf_cats(&enc, "KEY="), "KEY=") ;
    size_t before = enc.len ;
    TEST_ASSERT(environ_untrim_value(&enc, ""), "encode empty") ;
    TEST_ASSERT(enc.len == before, "empty value appends nothing") ;
    rt("", "empty round-trips to empty") ;
    TEST_END() ;
}

/* Values that MUST be quoted: the first encoded byte IS '"', and they
 * round-trip on both paths. Covers the QUOTE path and each whitespace/control
 * trigger of value_needs_quote, plus the per-byte escape switch. All cases here
 * are newline-FREE (newline cases live in test_newline_rejected). */
static void test_must_quote(void)
{
    TEST_START("test_must_quote") ;
    struct { char const *v ; char const *name ; } cases[] = {
        { " x",            "leading space" },
        { "x ",            "trailing space" },
        { "   ",           "all spaces" },
        { " ",             "single space" },
        { "a\"b",          "contains double quote" },
        { "a\\b",          "contains backslash" },
        { "a\tb",          "tab" },
        { "a\rb",          "carriage return" },
        { "a\fb",          "form feed" },
        { "\a\b\t\v\f\r",  "the 6 newline-free letter-escape controls together" },
        { "\x7f",          "DEL 0x7f" },
        { "\x01",          "SOH 0x01 (octal-escaped)" },
        { "\x1f",          "0x1f (octal-escaped)" },
        { "\" \\ \t \f \r # = ", "metachar soup (newline-free)" },
    } ;
    for (size_t i = 0 ; i < sizeof(cases)/sizeof(*cases) ; i++) {
        TEST_ASSERT(encoded_is_quoted(cases[i].v), cases[i].name) ;
        rt(cases[i].v, cases[i].name) ;
    }
    TEST_END() ;
}

/* Newline rejection: a value containing 0x0a is rejected (return 0, errno
 * EINVAL, nothing appended). environ_untrim_kv forwards the rejection (return
 * 0, errno EINVAL); per its contract it may leave "key=" partially appended. */
static void test_newline_rejected(void)
{
    TEST_START("test_newline_rejected") ;
    char const *nlv[] = { "a\nb", "\n", "x\n", "\ny", "a\nb\nc" } ;
    for (size_t i = 0 ; i < sizeof(nlv)/sizeof(*nlv) ; i++)
        assert_rejected(nlv[i], "newline value rejected by environ_untrim_value") ;

    /* environ_untrim_kv forwards the EINVAL rejection. */
    for (size_t i = 0 ; i < sizeof(nlv)/sizeof(*nlv) ; i++) {
        _alloc_strbuf_(line, 64) ;
        line.len = 0 ;
        errno = 0 ;
        int r = environ_untrim_kv(&line, "K", nlv[i]) ;
        TEST_ASSERT(r == 0, "format_entry rejects newline value") ;
        TEST_ASSERT(errno == EINVAL, "format_entry forwards EINVAL") ;
        /* contract: key and '=' may be partially appended, value/newline are not */
        TEST_ASSERT(line.len == 2 && !memcmp(line.s, "K=", 2),
                    "format_entry left only 'K=' (no value, no newline)") ;
    }
    TEST_END() ;
}

/* The exact escape forms: pin that environ_untrim_value emits the documented
 * canonical bytes, not just "something quoted". */
static void test_exact_escape_bytes(void)
{
    TEST_START("test_exact_escape_bytes") ;
    _alloc_strbuf_(enc, 64) ;

    /* the 6 newline-free letter escapes -> \a \b \t \v \f \r (0x0a rejected) */
    char const *got = encode_value(&enc, "\a\b\t\v\f\r") ;
    TEST_ASSERT(!strcmp(got, "\"\\a\\b\\t\\v\\f\\r\""), "letter escapes exact") ;

    /* '"' -> \" , '\\' -> \\ */
    got = encode_value(&enc, "\"\\") ;
    TEST_ASSERT(!strcmp(got, "\"\\\"\\\\\""), "quote and backslash escapes exact") ;

    /* 0x01 -> \001 (3-digit octal), 0x1f -> \037, 0x7f -> \177 */
    got = encode_value(&enc, "\x01") ;
    TEST_ASSERT(!strcmp(got, "\"\\001\""), "0x01 -> \\001") ;
    got = encode_value(&enc, "\x1f") ;
    TEST_ASSERT(!strcmp(got, "\"\\037\""), "0x1f -> \\037") ;
    got = encode_value(&enc, "\x7f") ;
    TEST_ASSERT(!strcmp(got, "\"\\177\""), "0x7f -> \\177") ;

    /* 0x06 (no letter escape) -> \006 octal */
    got = encode_value(&enc, "\x06") ;
    TEST_ASSERT(!strcmp(got, "\"\\006\""), "0x06 -> \\006") ;
    /* 0x0e (just past \r) -> \016 octal */
    got = encode_value(&enc, "\x0e") ;
    TEST_ASSERT(!strcmp(got, "\"\\016\""), "0x0e -> \\016") ;

    /* a non-special byte inside a quoted value is emitted literally */
    got = encode_value(&enc, " A") ;          /* leading space forces quoting */
    TEST_ASSERT(!strcmp(got, "\" A\""), "literal byte inside quotes") ;

    /* '#' and '=' are NOT escaped even inside quotes */
    got = encode_value(&enc, " #=") ;
    TEST_ASSERT(!strcmp(got, "\" #=\""), "# and = literal inside quotes") ;

    TEST_END() ;
}

/* Octal-cap trap: {0x01, '1'} must encode as \001 then a literal '1' so the
 * decoder's 3-digit octal cap does NOT swallow the following digit. */
static void test_octal_cap_trap(void)
{
    TEST_START("test_octal_cap_trap") ;
    char const buf[] = { 0x01, '1', 0 } ;
    _alloc_strbuf_(enc, 32) ;
    char const *got = encode_value(&enc, buf) ;
    TEST_ASSERT(!strcmp(got, "\"\\0011\""), "octal cap: \\001 then literal 1") ;
    roundtrip_trim(buf, 2, "octal-cap trap round-trips to {0x01,'1'}") ;

    /* same trap with a control whose octal ends in a digit followed by a digit:
     * 0x09 is \t (letter escape), so use 0x02 -> \002 then '7' (octal digit). */
    char const buf2[] = { 0x02, '7', 0 } ;
    got = encode_value(&enc, buf2) ;
    TEST_ASSERT(!strcmp(got, "\"\\0027\""), "octal cap: \\002 then literal 7") ;
    roundtrip_trim(buf2, 2, "octal-cap trap {0x02,'7'}") ;
    TEST_END() ;
}

/* EVERY single byte 0x01..0xff as a 1-char value. 0x0a is REJECTED (return 0,
 * EINVAL, nothing appended); every other byte round-trips via BOTH environ_trim
 * and environ_get_value. Exhaustive inverse proof over the whole accepted
 * alphabet. (0x00 excluded: cannot appear in a C string.) */
static void test_all_bytes(void)
{
    TEST_START("test_all_bytes") ;
    for (int b = 0x01 ; b <= 0xff ; b++) {
        char buf[2] = { (char)b, 0 } ;
        char msg[40] ;
        if (b == '\n') {
            snprintf(msg, sizeof msg, "byte 0x0a rejected") ;
            assert_rejected(buf, msg) ;
            continue ;
        }
        snprintf(msg, sizeof msg, "byte 0x%02x round-trips", b) ;
        roundtrip_trim(buf, 1, msg) ;
        roundtrip_get_value(buf, msg) ;
    }
    TEST_END() ;
}

/* Two-byte pairs of every byte against a small trailer set, to exercise escape
 * adjacency (octal cap, letter-escape boundaries, quote/backslash runs). A pair
 * containing 0x0a is asserted REJECTED; the rest round-trip via environ_trim. */
static void test_byte_pairs(void)
{
    TEST_START("test_byte_pairs") ;
    char const trailers[] = { '0', '7', '8', 'a', 'x', '\\', '"', 'A' } ;
    for (int b = 0x01 ; b <= 0xff ; b++) {
        for (size_t t = 0 ; t < sizeof trailers ; t++) {
            char buf[3] = { (char)b, trailers[t], 0 } ;
            if (b == '\n' || trailers[t] == '\n')   /* trailer set has no \n */
                assert_rejected(buf, "pair with newline rejected") ;
            else
                roundtrip_trim(buf, 2, "byte pair round-trips") ;
        }
    }
    /* explicit newline-in-second-position pairs (trailer set is newline-free) */
    for (int b = 0x01 ; b <= 0xff ; b++) {
        if (b == '\n') continue ;
        char buf[3] = { (char)b, '\n', 0 } ;
        assert_rejected(buf, "pair <byte>\\n rejected") ;
    }
    TEST_END() ;
}

/* environ_untrim_kv: appends exactly "key=<encoded value>\n", and the
 * result decodes (via environ_trim) to one entry with the right key
 * (environ_get_key) and the right value (environ_trim oracle). All value cases
 * here are newline-free (newline forwarding is in test_newline_rejected). */
static void test_format_entry(void)
{
    TEST_START("test_format_entry") ;
    struct { char const *k ; char const *v ; } cases[] = {
        { "TTY",   "tty1" },
        { "PATH",  "/usr/bin:/sbin" },
        { "MSG",   "hello world" },
        { "Q",     "a\"b\\c" },
        { "CTRL",  "x\ty\rz" },                       /* tab + CR, no newline */
        { "EMPTY", "" },
        { "META",  "\" \\ \t \f \r # = " },           /* newline-free soup */
    } ;
    for (size_t i = 0 ; i < sizeof(cases)/sizeof(*cases) ; i++) {
        char const *k = cases[i].k, *v = cases[i].v ;

        /* exact literal shape: key '=' encoded-value '\n' */
        _alloc_strbuf_(line, 64) ;
        line.len = 0 ;
        TEST_ASSERT(environ_untrim_kv(&line, k, v), "format_entry") ;

        _alloc_strbuf_(ref, 64) ;
        ref.len = 0 ;
        TEST_ASSERT(strbuf_cats(&ref, k), "ref key") ;
        TEST_ASSERT(strbuf_catb(&ref, "=", 1), "ref =") ;
        TEST_ASSERT(environ_untrim_value(&ref, v), "ref encode") ;
        TEST_ASSERT(strbuf_catb(&ref, "\n", 1), "ref newline") ;
        TEST_ASSERT(line.len == ref.len, "format_entry exact length") ;
        TEST_ASSERT(memcmp(line.s, ref.s, line.len) == 0,
                    "format_entry == key '=' encoded(value) '\\n'") ;
        TEST_ASSERT(line.s[line.len - 1] == '\n', "trailing newline") ;
        TEST_ASSERT(line.s[strlen(k)] == '=', "'=' right after key") ;

        /* decode the formatted line and check key + value */
        TEST_ASSERT(strbuf_uncounted(&line), "uncounted line") ;
        _alloc_strbuf_(dec, 64) ;
        TEST_ASSERT(environ_trim(&dec, line.s), "trim formatted line") ;

        _alloc_strbuf_(key, 32) ;
        TEST_ASSERT(environ_get_key(&key, dec.s), "get_key") ;
        TEST_ASSERT(strbuf_uncounted(&key), "uncounted key") ;
        TEST_ASSERT(!strcmp(key.s, k), "decoded key matches") ;

        /* value oracle: bytes after the key '=' up to the entry NUL */
        size_t klen = strlen(k) ;
        TEST_ASSERT(dec.s[klen] == '=', "decoded '=' position") ;
        char const *got = dec.s + klen + 1 ;
        size_t glen = dec.len - 1 - (klen + 1) ;
        TEST_ASSERT(glen == strlen(v), "decoded value length") ;
        TEST_ASSERT(memcmp(got, v, glen) == 0, "decoded value matches") ;
    }
    TEST_END() ;
}

/* Deterministic PRNG fuzz fallback: random NUL-free byte strings that DO include
 * 0x0a. Oracle: a value containing 0x0a must be rejected (return 0, EINVAL,
 * nothing appended); otherwise it must round-trip via environ_trim AND
 * environ_get_value. (A libFuzzer harness exercises the same property; this loop
 * guarantees coverage without libFuzzer and reaches the rejection path.) */
static uint64_t prng_state = 0x9e3779b97f4a7c15ULL ;
static uint32_t prng_next(void)
{
    prng_state ^= prng_state << 13 ;
    prng_state ^= prng_state >> 7 ;
    prng_state ^= prng_state << 17 ;
    return (uint32_t)(prng_state >> 32) ;
}

static void test_fuzz_prng(void)
{
    TEST_START("test_fuzz_prng") ;
    int saw_newline = 0 ;
    for (int iter = 0 ; iter < 20000 ; iter++) {
        char buf[64] ;
        size_t n = prng_next() % (sizeof buf - 1) ;
        for (size_t i = 0 ; i < n ; i++) {
            unsigned char c ;
            do { c = (unsigned char)(prng_next() & 0xff) ; }
            while (c == 0) ;                  /* no embedded NUL in a C string */
            buf[i] = (char)c ;
        }
        buf[n] = 0 ;
        size_t vlen = strlen(buf) ;           /* == n: no NUL was inserted */
        if (memchr(buf, '\n', vlen)) {
            saw_newline = 1 ;
            assert_rejected(buf, "prng fuzz: newline value rejected") ;
        } else {
            roundtrip_trim(buf, vlen, "prng fuzz trim round-trip") ;
            roundtrip_get_value(buf, "prng fuzz get_value round-trip") ;
        }
    }
    TEST_ASSERT(saw_newline, "prng must have produced at least one 0x0a") ;
    TEST_END() ;
}

int main(void)
{
    printf("=== environ_untrim_value / environ_untrim_kv test suite ===\n\n") ;

    test_clean_unquoted() ;
    test_empty() ;
    test_must_quote() ;
    test_newline_rejected() ;
    test_exact_escape_bytes() ;
    test_octal_cap_trap() ;
    test_all_bytes() ;
    test_byte_pairs() ;
    test_format_entry() ;
    test_fuzz_prng() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
