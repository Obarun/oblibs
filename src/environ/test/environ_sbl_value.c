/*
 * environ_sbl_value.c
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

/* Unit test for the internal helper environ_sbl_value(out, s, len, key).
 *
 * It walks the NUL-separated key=value entries in [s, s+len), exact-matches
 * the key (the bytes before the first '='), and copies that entry's value
 * VERBATIM via environ_get_value -- no re-decode, contrast environ_search_value
 * which re-runs environ_trim. The sbl inputs below are built two ways:
 *   - by hand, as raw "key=value\0...\0" byte literals (explicit, lets us
 *     express a value containing '=', a malformed no-'=' entry, etc.);
 *   - via environ_merge_string, to exercise the real decoded-sbl shape and to
 *     prove a value carrying a literal backslash is returned byte-for-byte.
 *
 * The length passed to environ_sbl_value is the TOTAL byte length of the
 * entries INCLUDING the final NUL, exactly as the production callers pass
 * (modifs->len / new->len after a strbuf holding the sbl). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

#include "../environ-internal.h"

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

/* A found lookup: returns 1, out holds @expect exactly (length + bytes). */
static void expect_found(char const *s, size_t len, char const *key, char const *expect)
{
    _alloc_strbuf_(out, 64) ;
    errno = 0 ;
    TEST_ASSERT(environ_sbl_value(&out, s, len, key) == 1, "lookup should be found") ;
    TEST_ASSERT(out.len == strlen(expect), "value length mismatch") ;
    /* compare with memcmp on the declared length, not strcmp: a value is never
     * supposed to hold an embedded NUL here, but assert by length anyway. */
    TEST_ASSERT(out.len == strlen(out.s), "value must be a clean C string") ;
    TEST_ASSERT(!memcmp(out.s, expect, out.len), "value content mismatch") ;
}

/* A not-found lookup: returns 0 and errno == EINVAL. */
static void expect_notfound(char const *s, size_t len, char const *key)
{
    _alloc_strbuf_(out, 64) ;
    errno = 0 ;
    TEST_ASSERT(environ_sbl_value(&out, s, len, key) == 0, "lookup should be not found") ;
    TEST_ASSERT(errno == EINVAL, "not found must set EINVAL") ;
}

/* Exact match, single entry, then a value containing '=' (only the FIRST '='
 * separates key from value -- the rest is part of the value). */
static void test_exact_match(void)
{
    TEST_START("test_exact_match") ;

    static char const s1[] = "A=hello" ;
    expect_found(s1, sizeof s1, "A", "hello") ;

    /* value itself contains '=' : A -> "a=b" (split on first '=' only) */
    static char const s2[] = "A=a=b" ;
    expect_found(s2, sizeof s2, "A", "a=b") ;

    TEST_END() ;
}

/* Multiple entries: each key resolves to its own value, regardless of order. */
static void test_multiple_entries(void)
{
    TEST_START("test_multiple_entries") ;

    static char const s[] = "A=1\0B=2\0C=three" ;
    size_t len = sizeof s ;  /* includes the final NUL of "three" */

    expect_found(s, len, "A", "1") ;
    expect_found(s, len, "B", "2") ;
    expect_found(s, len, "C", "three") ;

    TEST_END() ;
}

/* Prefix collision: an entry FOOBAR must NOT be returned for a lookup of FOO,
 * nor must FOO be returned for FOOBAR / FO. Only the exact key matches. This is
 * the r == klen exact-length guard. */
static void test_prefix_collision(void)
{
    TEST_START("test_prefix_collision") ;

    static char const s[] = "FOO=short\0FOOBAR=long" ;
    size_t len = sizeof s ;

    expect_found(s, len, "FOO", "short") ;       /* exact, not FOOBAR's value */
    expect_found(s, len, "FOOBAR", "long") ;     /* exact, not FOO's value */
    expect_notfound(s, len, "FO") ;              /* shorter prefix: no match */
    expect_notfound(s, len, "FOOB") ;            /* mid prefix: no match */
    expect_notfound(s, len, "FOOBARX") ;         /* longer: no match */

    /* reversed order: FOOBAR first must still not shadow FOO */
    static char const s2[] = "FOOBAR=long\0FOO=short" ;
    expect_found(s2, sizeof s2, "FOO", "short") ;

    TEST_END() ;
}

/* An entry with no '=' is SKIPPED (not matched, not an error). A following
 * valid entry is still found; looking up the malformed key yields not-found. */
static void test_no_equal_skipped(void)
{
    TEST_START("test_no_equal_skipped") ;

    static char const s[] = "NOEQ\0A=1" ;
    size_t len = sizeof s ;

    expect_found(s, len, "A", "1") ;        /* entry after the malformed one */
    expect_notfound(s, len, "NOEQ") ;       /* malformed entry never matches */

    /* malformed entry in the MIDDLE: walking must continue past it */
    static char const s2[] = "A=1\0NOEQ\0B=2" ;
    expect_found(s2, sizeof s2, "B", "2") ;
    expect_notfound(s2, sizeof s2, "NOEQ") ;

    TEST_END() ;
}

/* Empty value "K=" -> found, out empty (len 0). */
static void test_empty_value(void)
{
    TEST_START("test_empty_value") ;

    static char const s[] = "K=" ;
    expect_found(s, sizeof s, "K", "") ;

    /* empty value between two others */
    static char const s2[] = "A=1\0K=\0B=2" ;
    expect_found(s2, sizeof s2, "K", "") ;
    expect_found(s2, sizeof s2, "A", "1") ;
    expect_found(s2, sizeof s2, "B", "2") ;

    TEST_END() ;
}

/* Not found in a non-empty sbl, and the empty-sbl edge (len 0): both 0/EINVAL. */
static void test_not_found(void)
{
    TEST_START("test_not_found") ;

    static char const s[] = "A=1\0B=2" ;
    expect_notfound(s, sizeof s, "C") ;
    expect_notfound(s, sizeof s, "") ;       /* empty key matches nothing here */

    /* empty sbl: the while loop never runs -> EINVAL */
    expect_notfound("", 0, "A") ;

    TEST_END() ;
}

/* NO re-decode: build the sbl through the real parser (environ_merge_string ->
 * environ_trim), so the stored value of K is a single literal backslash byte;
 * environ_sbl_value must return it byte-for-byte. environ_search_value on the
 * same raw text would re-trim and the backslash would not survive a second
 * pass -- that's exactly the bug this helper avoids. */
static void test_no_redecode_backslash(void)
{
    TEST_START("test_no_redecode_backslash") ;

    _cleanup_strbuf_ strbuf modifs = STRBUF_ZERO ;
    /* raw file content: K="a\\b"  -> trim decodes \\ to one '\'  -> value a\b */
    TEST_ASSERT(environ_merge_string(&modifs, "K=\"a\\\\b\"") == 1, "merge_string ok") ;
    TEST_ASSERT(!strcmp(modifs.s, "K=a\\b"), "trim decoded \\\\ to one backslash") ;

    /* modifs.len is the sbl byte length including the final NUL */
    expect_found(modifs.s, modifs.len, "K", "a\\b") ;

    TEST_END() ;
}

/* NO re-decode of a literal double-quote, same mechanism via \" in the source. */
static void test_no_redecode_quote(void)
{
    TEST_START("test_no_redecode_quote") ;

    _cleanup_strbuf_ strbuf modifs = STRBUF_ZERO ;
    /* raw: K="a\"b"  -> trim decodes \" to one '"'  -> value a"b */
    TEST_ASSERT(environ_merge_string(&modifs, "K=\"a\\\"b\"") == 1, "merge_string ok") ;
    TEST_ASSERT(!strcmp(modifs.s, "K=a\"b"), "trim decoded \\\" to one quote") ;

    expect_found(modifs.s, modifs.len, "K", "a\"b") ;

    TEST_END() ;
}

int main(void)
{
    test_exact_match() ;
    test_multiple_entries() ;
    test_prefix_collision() ;
    test_no_equal_skipped() ;
    test_empty_value() ;
    test_not_found() ;
    test_no_redecode_backslash() ;
    test_no_redecode_quote() ;

    printf("\nAll environ_sbl_value tests passed.\n") ;
    return 0 ;
}
