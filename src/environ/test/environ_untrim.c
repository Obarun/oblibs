/*
 * environ_untrim.c
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

/* Tests for environ_untrim, the exact file-level inverse of environ_trim.
 * environ_untrim walks the NUL-separated key=value entries of an sbl (as
 * produced by environ_trim) and appends, per entry, "key=<encoded value>\n"
 * to @out, the value re-encoded via environ_untrim_value.
 *
 * THE oracle is the round-trip property: for any envfile text T, with
 * S = environ_trim(T), parsing environ_untrim(S) back with environ_trim
 * reproduces S byte-for-byte. The bulk of the suite drives a corpus of envfile
 * texts through trim -> untrim -> trim and asserts the second trim equals the
 * first byte-for-byte; a couple of cases also pin the exact textual shape of
 * the untrim output (notably one '\n' per entry, key verbatim).
 *
 * The two failure paths (entry with no '=', value bearing 0x0a) are pinned with
 * hand-built sbl buffers: environ_trim can never emit either, so they cannot be
 * reached through the round-trip and must be constructed directly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

/* Build an sbl by hand: a NUL-separated list of @n entries, NUL not counted in
 * len (exactly what environ_trim leaves via strbuf_uncounted). The terminating
 * NUL of the last entry is present in the buffer but not in len, matching the
 * sbl invariant. */
static void build_sbl(strbuf *out, char const *const *entries, size_t n)
{
    for (size_t i = 0 ; i < n ; i++) {
        TEST_ASSERT(strbuf_catb(out, entries[i], strlen(entries[i])), "build_sbl cat entry") ;
        TEST_ASSERT(strbuf_catb(out, "\0", 1), "build_sbl cat NUL") ;
    }
    /* drop the final NUL from the count, keep it in the buffer: sbl invariant. */
    if (n)
        out->len-- ;
    TEST_ASSERT(strbuf_uncounted(out), "build_sbl uncounted") ;
}

/* The round-trip oracle. Trim @text into S, untrim S into U, trim U into S2,
 * then assert S2 == S byte-for-byte (same len, same bytes). */
static void roundtrip(char const *text, char const *what)
{
    _alloc_strbuf_(s, 256) ;
    TEST_ASSERT(environ_trim(&s, text), what) ;

    _alloc_strbuf_(u, 256) ;
    TEST_ASSERT(environ_untrim(&u, &s), what) ;
    TEST_ASSERT(strbuf_uncounted(&u), "untrim output uncounted") ;

    _alloc_strbuf_(s2, 256) ;
    TEST_ASSERT(environ_trim(&s2, u.s), what) ;

    TEST_ASSERT(s2.len == s.len, "round-trip length differs") ;
    TEST_ASSERT(memcmp(s2.s, s.s, s.len) == 0, "round-trip bytes differ") ;
}

/* Multiple clean entries that stay unquoted on re-encode. */
static void test_rt_multiple(void)
{
    TEST_START("test_rt_multiple") ;
    roundtrip("A=1\nB=2\nC=3", "A=1 B=2 C=3") ;
    TEST_END() ;
}

/* Clean values kept verbatim: tty, paths, ":0", internal spaces, '#'/'=' inside
 * a value -- none of these force quoting. */
static void test_rt_clean_values(void)
{
    TEST_START("test_rt_clean_values") ;
    roundtrip("TTY=tty1", "tty1") ;
    roundtrip("PATH=/usr/bin:/usr/sbin:/bin", "path") ;
    roundtrip("DISPLAY=:0", ":0") ;
    roundtrip("A=hello world", "internal space") ;
    roundtrip("A=a#b", "hash inside value") ;
    /* "A=a=b": first '=' splits key/value; value "a=b" survives unquoted. */
    roundtrip("A=a=b", "equals inside value") ;
    TEST_END() ;
}

/* Values that force quoting on re-encode: leading/trailing space, embedded '"'
 * and '\\', tabs/CR/FF and other control bytes, 0x7f, UTF-8. Each must still
 * round-trip byte-for-byte. */
static void test_rt_quoting(void)
{
    TEST_START("test_rt_quoting") ;

    /* leading / trailing space (only survives trim when quoted in the source). */
    roundtrip("A=\"  x\"", "leading space") ;
    roundtrip("A=\"x  \"", "trailing space") ;
    roundtrip("A=\"  x  \"", "lead+trail space") ;

    /* embedded quote and backslash. */
    roundtrip("A=\"a\\\"b\"", "embedded quote") ;
    roundtrip("A=\"a\\\\b\"", "embedded backslash") ;

    /* control bytes via escapes: TAB, CR, FF, VT, BEL, BS. */
    roundtrip("A=\"a\\tb\"", "embedded tab") ;
    roundtrip("A=\"a\\rb\"", "embedded CR") ;
    roundtrip("A=\"a\\fb\"", "embedded FF") ;
    roundtrip("A=\"a\\vb\"", "embedded VT") ;
    roundtrip("A=\"a\\ab\"", "embedded BEL") ;
    roundtrip("A=\"a\\bb\"", "embedded BS") ;

    /* generic control byte (0x01) -> octal escape path. */
    roundtrip("A=\"a\\001b\"", "embedded 0x01") ;
    /* 0x1f, the top of the control range. */
    roundtrip("A=\"a\\037b\"", "embedded 0x1f") ;
    /* 0x7f DEL. */
    roundtrip("A=\"a\\177b\"", "embedded 0x7f") ;
    /* an octal escape immediately followed by an octal digit: the 3-digit cap
     * keeps it unambiguous on re-decode. */
    roundtrip("A=\"\\0017\"", "octal then octal digit") ;

    /* UTF-8 (cafe with e-acute) stays raw -- bytes >= 0x80 are not quoted. */
    roundtrip("A=caf\xc3\xa9", "UTF-8 raw") ;
    /* UTF-8 inside an otherwise-quoted value. */
    roundtrip("A=\" caf\xc3\xa9 \"", "UTF-8 quoted") ;

    TEST_END() ;
}

/* Empty value, single entry, and the empty sbl. */
static void test_rt_edges(void)
{
    TEST_START("test_rt_edges") ;
    roundtrip("A=", "empty value") ;
    roundtrip("ONLY=one", "single entry") ;
    roundtrip("", "empty input -> empty sbl") ;
    TEST_END() ;
}

/* Many entries, mixing raw and quoted, in one buffer. */
static void test_rt_many(void)
{
    TEST_START("test_rt_many") ;
    char const *conf =
        "VERBOSITY=1\n"
        "PATH=/usr/bin:/sbin\n"
        "TZ=GMT\n"
        "EMPTY=\n"
        "SPACED=\"  pad  \"\n"
        "CTRL=\"x\\ty\"\n"
        "QUOTE=\"a\\\"b\"\n"
        "HASH=a#b\n"
        "BANG=!yes\n" ;
    roundtrip(conf, "many mixed entries") ;
    TEST_END() ;
}

/* Exact textual shape of the untrim output: key verbatim, '=' separator, one
 * '\n' per entry (including the last). This pins the literal bytes, not just the
 * round-trip equivalence. */
static void test_exact_shape(void)
{
    TEST_START("test_exact_shape") ;

    /* "A=1\nB=2" -> sbl {A=1, B=2} -> "A=1\nB=2\n" (trailing newline, one per
     * entry). */
    {
        _alloc_strbuf_(s, 64) ;
        TEST_ASSERT(environ_trim(&s, "A=1\nB=2"), "trim") ;
        _alloc_strbuf_(u, 64) ;
        TEST_ASSERT(environ_untrim(&u, &s), "untrim") ;
        TEST_ASSERT(u.len == strlen("A=1\nB=2\n"), "untrim two entries length") ;
        TEST_ASSERT(memcmp(u.s, "A=1\nB=2\n", u.len) == 0, "untrim two entries bytes") ;
    }

    /* single entry: exactly "K=v\n". */
    {
        _alloc_strbuf_(s, 64) ;
        TEST_ASSERT(environ_trim(&s, "K=v"), "trim single") ;
        _alloc_strbuf_(u, 64) ;
        TEST_ASSERT(environ_untrim(&u, &s), "untrim single") ;
        TEST_ASSERT(u.len == strlen("K=v\n"), "single entry length") ;
        TEST_ASSERT(memcmp(u.s, "K=v\n", u.len) == 0, "single entry bytes") ;
    }

    /* empty value: exactly "K=\n". */
    {
        _alloc_strbuf_(s, 64) ;
        TEST_ASSERT(environ_trim(&s, "K="), "trim empty val") ;
        _alloc_strbuf_(u, 64) ;
        TEST_ASSERT(environ_untrim(&u, &s), "untrim empty val") ;
        TEST_ASSERT(u.len == strlen("K=\n"), "empty value length") ;
        TEST_ASSERT(memcmp(u.s, "K=\n", u.len) == 0, "empty value bytes") ;
    }

    /* a value that must be quoted: "K= x " (leading+trailing space) ->
     * exactly "K=\" x \"\n". */
    {
        _alloc_strbuf_(s, 64) ;
        TEST_ASSERT(environ_trim(&s, "K=\" x \""), "trim spaced") ;
        TEST_ASSERT(!strcmp(s.s, "K= x "), "trim gives raw spaced value") ;
        _alloc_strbuf_(u, 64) ;
        TEST_ASSERT(environ_untrim(&u, &s), "untrim spaced") ;
        TEST_ASSERT(u.len == strlen("K=\" x \"\n"), "quoted value length") ;
        TEST_ASSERT(memcmp(u.s, "K=\" x \"\n", u.len) == 0, "quoted value bytes") ;
    }

    TEST_END() ;
}

/* untrim appends to @out, it does NOT clear it. Pre-seeding @out must leave the
 * prefix intact and append after it. */
static void test_appends_not_clears(void)
{
    TEST_START("test_appends_not_clears") ;
    _alloc_strbuf_(s, 64) ;
    TEST_ASSERT(environ_trim(&s, "A=1"), "trim") ;
    _alloc_strbuf_(u, 64) ;
    TEST_ASSERT(strbuf_cats(&u, "PRE:"), "seed out") ;
    TEST_ASSERT(environ_untrim(&u, &s), "untrim onto seeded out") ;
    TEST_ASSERT(u.len == strlen("PRE:A=1\n"), "appended length") ;
    TEST_ASSERT(memcmp(u.s, "PRE:A=1\n", u.len) == 0, "prefix preserved + appended") ;
    TEST_END() ;
}

/* Failure path 1: an sbl entry with no '='. get_len_until returns -1 ->
 * EINVAL, return 0, and @out must be left untouched (untrim returns before any
 * append for that entry; here it is the first entry so out stays empty). */
static void test_fail_no_equals(void)
{
    TEST_START("test_fail_no_equals") ;
    _alloc_strbuf_(sbl, 32) ;
    char const *entries[] = { "NOEQ" } ;
    build_sbl(&sbl, entries, 1) ;

    _alloc_strbuf_(out, 32) ;
    errno = 0 ;
    TEST_ASSERT(environ_untrim(&out, &sbl) == 0, "no '=' must fail") ;
    TEST_ASSERT(errno == EINVAL, "no '=' sets EINVAL") ;
    TEST_ASSERT(out.len == 0, "no '=' leaves out empty") ;
    TEST_END() ;
}

/* Failure path 1b: a good entry first, then a no-'=' entry. The good entry is
 * appended, then the second fails with EINVAL: proves the loop advances past
 * the first entry and the '=' check runs per entry. */
static void test_fail_no_equals_second(void)
{
    TEST_START("test_fail_no_equals_second") ;
    _alloc_strbuf_(sbl, 32) ;
    char const *entries[] = { "A=1", "NOEQ" } ;
    build_sbl(&sbl, entries, 2) ;

    _alloc_strbuf_(out, 32) ;
    errno = 0 ;
    TEST_ASSERT(environ_untrim(&out, &sbl) == 0, "second entry no '=' fails") ;
    TEST_ASSERT(errno == EINVAL, "second entry no '=' sets EINVAL") ;
    /* the first entry was already appended before the second failed. */
    TEST_ASSERT(out.len == strlen("A=1\n"), "first entry appended before failure") ;
    TEST_ASSERT(memcmp(out.s, "A=1\n", out.len) == 0, "first entry bytes before failure") ;
    TEST_END() ;
}

/* Failure path 2: an sbl entry whose value contains 0x0a. environ_untrim_value
 * rejects it (forwarded EINVAL). environ_trim can never emit such an entry, so
 * the sbl is built by hand. */
static void test_fail_newline_value(void)
{
    TEST_START("test_fail_newline_value") ;
    _alloc_strbuf_(sbl, 32) ;
    char const *entries[] = { "K=a\nb" } ;   /* value "a\nb" contains 0x0a */
    build_sbl(&sbl, entries, 1) ;

    _alloc_strbuf_(out, 32) ;
    errno = 0 ;
    TEST_ASSERT(environ_untrim(&out, &sbl) == 0, "newline in value must fail") ;
    TEST_ASSERT(errno == EINVAL, "newline in value sets EINVAL (forwarded)") ;
    /* environ_untrim_kv appends "K=" before calling environ_untrim_value, which
     * then fails: the partial "K=" prefix is the documented append-as-you-go
     * behaviour. We assert no '\n' line was completed for this entry. */
    TEST_ASSERT(memchr(out.s, '\n', out.len) == NULL, "no completed line on newline-value failure") ;
    TEST_END() ;
}

/* '=' with an empty key: ""=value. get_len_until returns 0 (the '=' is at
 * index 0), so eq==0 is NOT the error path; the key is empty and the value is
 * re-encoded. This pins eq>=0 (not eq>0) as the accept condition and the
 * eq vs eq+1 split point. */
static void test_empty_key(void)
{
    TEST_START("test_empty_key") ;
    _alloc_strbuf_(sbl, 32) ;
    char const *entries[] = { "=v" } ;   /* empty key, value "v" */
    build_sbl(&sbl, entries, 1) ;

    _alloc_strbuf_(out, 32) ;
    TEST_ASSERT(environ_untrim(&out, &sbl), "empty key accepted (eq==0)") ;
    TEST_ASSERT(out.len == strlen("=v\n"), "empty key output length") ;
    TEST_ASSERT(memcmp(out.s, "=v\n", out.len) == 0, "empty key output bytes") ;
    TEST_END() ;
}

int main(void)
{
    printf("=== environ_untrim test suite ===\n\n") ;

    test_rt_multiple() ;
    test_rt_clean_values() ;
    test_rt_quoting() ;
    test_rt_edges() ;
    test_rt_many() ;
    test_exact_shape() ;
    test_appends_not_clears() ;
    test_fail_no_equals() ;
    test_fail_no_equals_second() ;
    test_fail_newline_value() ;
    test_empty_key() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
