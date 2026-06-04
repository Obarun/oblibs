/*
 * environ_get_value.c
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

/* Non-regression test for environ_get_value as a pure value extractor
 * (everything after the first '=' up to '\n' or EOF) and for the
 * environ_search_value path that owns the single environ_trim parse.
 *
 * Regression guarded: environ_get_value used to re-run environ_trim on its
 * input. Callers (environ_substitute, parse_store_environ, 66-dbus-launch,
 * ...) hand it lines that were ALREADY parsed by environ_trim upstream, so a
 * value carrying a literal backslash (legitimately produced by the first
 * parse) was double-unescaped or rejected as a syntax error on the second
 * pass. The pure extractor must return such values verbatim. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <skalibs/stralloc.h>

#include <oblibs/environ.h>
#include <oblibs/stack.h>
#include <oblibs/sastr.h>

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

/* helper: run environ_get_value on @line, assert value equals @expect */
static void expect_value(char const *line, char const *expect)
{
    _alloc_stk_(val, strlen(line) + 1) ;
    TEST_ASSERT(environ_get_value(&val, line) == 1, "environ_get_value should succeed") ;
    TEST_ASSERT(val.len == strlen(expect), "value length mismatch") ;
    TEST_ASSERT(!strcmp(val.s, expect), "value content mismatch") ;
}

/* Basic extraction: after the first '=', verbatim. */
static void test_basic(void)
{
    TEST_START("test_basic") ;

    expect_value("KEY=hello world", "hello world") ;   /* internal spaces kept */
    expect_value("KEY=value", "value") ;
    expect_value("KEY=", "") ;                          /* empty value */
    expect_value("KEY=a=b=c", "a=b=c") ;               /* only first '=' is the separator */

    TEST_END() ;
}

/* The double-trim regression: an already-parsed value containing a literal
 * backslash must come back untouched, not re-interpreted nor rejected. */
static void test_literal_backslash(void)
{
    TEST_START("test_literal_backslash") ;

    /* literal C strings below: the value really holds a backslash byte */
    expect_value("KEY=a\\b", "a\\b") ;
    expect_value("KEY=a\\nb", "a\\nb") ;
    expect_value("PATH=C:\\tmp", "C:\\tmp") ;

    TEST_END() ;
}

/* Value extraction stops at the first '\n' (EOF-or-newline contract). */
static void test_stops_at_newline(void)
{
    TEST_START("test_stops_at_newline") ;

    expect_value("KEY=first\nKEY2=second", "first") ;

    TEST_END() ;
}

/* A line with no '=' has no value: must fail with EINVAL. */
static void test_no_separator(void)
{
    TEST_START("test_no_separator") ;

    _alloc_stk_(val, 16) ;
    errno = 0 ;
    TEST_ASSERT(environ_get_value(&val, "novalue") == 0, "no '=' must fail") ;
    TEST_ASSERT(errno == EINVAL, "no '=' must set EINVAL") ;

    TEST_END() ;
}

/* End-to-end: the real caller shape. environ_merge_string runs environ_trim
 * once (escaped backslash -> literal backslash in modifs), then we extract the
 * value from the already-parsed line, exactly as environ_substitute does. */
static void test_end_to_end_merge(void)
{
    TEST_START("test_end_to_end_merge") ;

    _alloc_sa_(modifs) ;
    /* raw file content: KEY="a\\b"  ->  parsed value is  a\b  (one backslash) */
    TEST_ASSERT(environ_merge_string(&modifs, "KEY=\"a\\\\b\"") == 1, "merge_string should succeed") ;
    TEST_ASSERT(!strcmp(modifs.s, "KEY=a\\b"), "merge_string parse result") ;

    expect_value(modifs.s, "a\\b") ;   /* must NOT double-unescape nor fail */

    TEST_END() ;
}

/* environ_search_value owns the single parse now: raw, quoted/escaped input
 * must be parsed exactly once and the requested key's value returned. */
static void test_search_value_raw(void)
{
    TEST_START("test_search_value_raw") ;

    char const *raw = "FOO=plain\nBAR=\"a\\\\b\"\nBAZ=last" ;

    _alloc_stk_(v, strlen(raw) + 1) ;

    TEST_ASSERT(environ_search_value(&v, raw, "FOO") == 1, "search FOO") ;
    TEST_ASSERT(!strcmp(v.s, "plain"), "FOO value") ;

    TEST_ASSERT(environ_search_value(&v, raw, "BAR") == 1, "search BAR") ;
    TEST_ASSERT(!strcmp(v.s, "a\\b"), "BAR value parsed once") ;

    TEST_ASSERT(environ_search_value(&v, raw, "BAZ") == 1, "search BAZ") ;
    TEST_ASSERT(!strcmp(v.s, "last"), "BAZ value") ;

    errno = 0 ;
    TEST_ASSERT(environ_search_value(&v, raw, "NOPE") == 0, "missing key must fail") ;

    TEST_END() ;
}

int main(void)
{
    test_basic() ;
    test_literal_backslash() ;
    test_stops_at_newline() ;
    test_no_separator() ;
    test_end_to_end_merge() ;
    test_search_value_raw() ;

    printf("\nAll environ_get_value tests passed.\n") ;
    return 0 ;
}
