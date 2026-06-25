/*
 * environ_trim.c
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

/* Tests for environ_trim, the envfile parser (execline envfile algorithm,
 * https://skarnet.org/software/execline/envfile.html). It is heavily used, so
 * coverage is broad and pins the documented behavior:
 *   - key=value entries, NUL-separated in the stack;
 *   - '=' may be surrounded by spaces; leading/trailing whitespace stripped
 *     from unquoted values, preserved inside double quotes;
 *   - '#' comment lines (even after whitespace) and blank lines ignored;
 *   - inside quotes: C escapes incl. the \a\b\t\n\v\f\r control letters (the
 *     code reimplemented with memchr), \\ , \" , hex \xNN and octal \NNN.
 * Expected outputs were captured from the real implementation. */

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

/* Compare the NUL-separated entries of @stk against @expected[0..n), and verify
 * there are exactly n of them (no extra, no missing). Values must not contain
 * an embedded NUL (none of the test cases do). */
static void check_entries(strbuf *stk, char const *const *expected, size_t n)
{
    char const *p = stk->s ;
    for (size_t i = 0 ; i < n ; i++) {
        TEST_ASSERT(!strcmp(p, expected[i]), expected[i]) ;
        p += strlen(p) + 1 ;
    }
    TEST_ASSERT((size_t)(p - stk->s) == stk->len, "exact entry count") ;
}

/* key=value lines, newline-separated, last line without a newline. */
static void test_basic(void)
{
    TEST_START("test_basic") ;
    _alloc_strbuf_(stk, 128) ;
    TEST_ASSERT(environ_trim(&stk, "A=1\nB=2\nC=3"), "trim") ;
    char const *exp[] = { "A=1", "B=2", "C=3" } ;
    check_entries(&stk, exp, 3) ;
    TEST_END() ;
}

/* The reimplemented escape branch: each letter -> its control byte. The \n
 * letter escape is NOT tested here: a decoded newline is rejected (see
 * test_newline_rejected); the other six letters map to their control byte. */
static void test_escapes(void)
{
    TEST_START("test_escapes") ;
    _alloc_strbuf_(stk, 64) ;
    TEST_ASSERT(environ_trim(&stk, "K=\"\\a\\b\\t\\v\\f\\r\""), "trim") ;
    TEST_ASSERT(!strcmp(stk.s, "K=\a\b\t\v\f\r"), "escape mapping") ;
    TEST_ASSERT((unsigned char)stk.s[2] == 7,  "\\a -> BEL") ;
    TEST_ASSERT((unsigned char)stk.s[3] == 8,  "\\b -> BS") ;
    TEST_ASSERT((unsigned char)stk.s[4] == 9,  "\\t -> TAB") ;
    TEST_ASSERT((unsigned char)stk.s[5] == 11, "\\v -> VT") ;
    TEST_ASSERT((unsigned char)stk.s[6] == 12, "\\f -> FF") ;
    TEST_ASSERT((unsigned char)stk.s[7] == 13, "\\r -> CR") ;
    TEST_END() ;
}

/* A value that decodes to a reserved separator byte -- NUL (0x00, the sbl entry
 * separator) or newline (0x0a, the line separator) -- is rejected, whatever
 * escape form produced it: the \n letter escape, or octal/hex (\0 \012 \x00 \x0a).
 * (A literal newline inside quotes is already a syntax error, and a literal NUL
 * cannot occur in the C-string input; this pins the decoded-byte rejection added
 * on top.) On rejection the stack is rolled back to entry. */
static void test_separator_byte_rejected(void)
{
    TEST_START("test_separator_byte_rejected") ;

    char const *cases[] = {
        "K=\"a\\nb\"",      /* \n letter escape -> 0x0a */
        "K=\"a\\012b\"",    /* octal \012 == 0x0a */
        "K=\"a\\x0ab\"",    /* hex \x0a */
        "K=\"\\n\"",        /* value is just a newline */
        "A=1\nK=\"x\\ny\"", /* a good entry then a newline-bearing one */
        "K=\"a\\0b\"",      /* octal \0 == 0x00 (sbl separator) */
        "K=\"a\\000b\"",    /* octal \000 == 0x00 */
        "K=\"a\\x00b\"",    /* hex \x00 */
        "K=\"\\0\"",        /* value is just a NUL */
        "A=1\nK=\"x\\000y\"", /* a good entry then a NUL-bearing one */
    } ;
    for (size_t i = 0 ; i < sizeof(cases)/sizeof(*cases) ; i++) {
        _alloc_strbuf_(stk, 64) ;
        TEST_ASSERT(environ_trim(&stk, cases[i]) == 0, cases[i]) ;
        TEST_ASSERT(stk.len == 0, "stack rolled back on separator-byte rejection") ;
    }

    /* the same escapes that do NOT produce 0x0a are still accepted: octal/hex
     * of a non-newline byte, and the \n-adjacent octal \011 (TAB) / \013 (VT). */
    {
        _alloc_strbuf_(stk, 64) ;
        TEST_ASSERT(environ_trim(&stk, "K=\"a\\011b\\013c\""), "non-newline escapes ok") ;
        TEST_ASSERT(!strcmp(stk.s, "K=a\tb\vc"), "octal \\011 -> TAB, \\013 -> VT") ;
    }
    TEST_END() ;
}

/* \\ -> backslash, \" -> quote (the documented cmd_args shape). */
static void test_backslash_quote(void)
{
    TEST_START("test_backslash_quote") ;
    _alloc_strbuf_(stk, 64) ;
    TEST_ASSERT(environ_trim(&stk, "K=\"a\\\\b\\\"c\""), "trim") ;
    TEST_ASSERT(!strcmp(stk.s, "K=a\\b\"c"), "literal backslash and quote") ;

    /* doc example: cmd_args=-g \"daemon off;\"  ->  -g "daemon off;" */
    _alloc_strbuf_(stk2, 64) ;
    TEST_ASSERT(environ_trim(&stk2, "cmd_args=-g \\\"daemon off;\\\""), "trim cmd_args") ;
    TEST_ASSERT(!strcmp(stk2.s, "cmd_args=-g \"daemon off;\""), "escaped quotes value") ;
    TEST_END() ;
}

/* hex \xNN and octal \NNN inside quotes. */
static void test_hex_octal(void)
{
    TEST_START("test_hex_octal") ;
    _alloc_strbuf_(stk, 64) ;
    TEST_ASSERT(environ_trim(&stk, "K=\"\\x41\\x42\""), "trim hex") ;
    TEST_ASSERT(!strcmp(stk.s, "K=AB"), "hex \\x41\\x42 -> AB") ;

    _alloc_strbuf_(stk2, 64) ;
    TEST_ASSERT(environ_trim(&stk2, "K=\"\\101\\102\""), "trim octal") ;
    TEST_ASSERT(!strcmp(stk2.s, "K=AB"), "octal \\101\\102 -> AB") ;
    TEST_END() ;
}

/* '=' may be spaced; unquoted values are whitespace-stripped, quoted ones keep
 * their leading/trailing whitespace; internal spaces are kept either way. */
static void test_whitespace(void)
{
    TEST_START("test_whitespace") ;

    _alloc_strbuf_(a, 64) ;
    TEST_ASSERT(environ_trim(&a, "A = 1"), "spaced =") ;
    TEST_ASSERT(!strcmp(a.s, "A=1"), "spaces around = stripped") ;

    _alloc_strbuf_(b, 64) ;
    TEST_ASSERT(environ_trim(&b, "A=  1   "), "lead/trail ws") ;
    TEST_ASSERT(!strcmp(b.s, "A=1"), "unquoted value ws stripped") ;

    _alloc_strbuf_(c, 64) ;
    TEST_ASSERT(environ_trim(&c, "A=\"  x  \""), "quoted ws") ;
    TEST_ASSERT(!strcmp(c.s, "A=  x  "), "quoted value ws preserved") ;

    _alloc_strbuf_(d, 64) ;
    TEST_ASSERT(environ_trim(&d, "A=hello world"), "internal space") ;
    TEST_ASSERT(!strcmp(d.s, "A=hello world"), "internal space kept") ;

    TEST_END() ;
}

/* comments (even after whitespace) and blank lines are ignored. */
static void test_comments_blanks(void)
{
    TEST_START("test_comments_blanks") ;
    _alloc_strbuf_(stk, 128) ;
    TEST_ASSERT(environ_trim(&stk, "# header\n\nVERBOSITY=1\n   # indented\nTREE=boot\n"), "trim") ;
    char const *exp[] = { "VERBOSITY=1", "TREE=boot" } ;
    check_entries(&stk, exp, 2) ;
    TEST_END() ;
}

/* empty value is kept; empty input yields no entries. */
static void test_empty(void)
{
    TEST_START("test_empty") ;

    _alloc_strbuf_(a, 32) ;
    TEST_ASSERT(environ_trim(&a, "A="), "empty value") ;
    char const *exp[] = { "A=" } ;
    check_entries(&a, exp, 1) ;

    _alloc_strbuf_(b, 32) ;
    TEST_ASSERT(environ_trim(&b, ""), "empty input") ;
    TEST_ASSERT(b.len == 0, "no entries for empty input") ;

    TEST_END() ;
}

/* a realistic init.conf-shaped block. */
static void test_real_world(void)
{
    TEST_START("test_real_world") ;
    _alloc_strbuf_(stk, 256) ;
    char const *conf =
        "VERBOSITY=1\n"
        "PATH=/usr/bin:/usr/sbin:/bin:/sbin\n"
        "TREE=boot\n"
        "UMASK=0022\n"
        "RESCAN=0\n" ;
    TEST_ASSERT(environ_trim(&stk, conf), "trim conf") ;
    char const *exp[] = {
        "VERBOSITY=1",
        "PATH=/usr/bin:/usr/sbin:/bin:/sbin",
        "TREE=boot",
        "UMASK=0022",
        "RESCAN=0",
    } ;
    check_entries(&stk, exp, 5) ;
    TEST_END() ;
}

/* Real /etc/66/conf/boot@system shape: '##'/'#' comment blocks, commented-out
 * keys, and KEY=!value entries where '!' (66's "do not export" marker) is just
 * part of the value as far as the parser is concerned. */
static void test_boot_system_shape(void)
{
    TEST_START("test_boot_system_shape") ;
    _alloc_strbuf_(stk, 512) ;
    char const *conf =
        "## [STARTWARN]\n"
        "## DO NOT MODIFY THIS FILE.\n"
        "#\n"
        "## Set your HOSTNAME.\n"
        "HOSTNAME=obarunS6\n"
        "## Set your timezone.\n"
        "TZ=GMT\n"
        "## Number of tty(s).\n"
        "TTY=!2\n"
        "## Console font.\n"
        "#FONT_MAP=\n"
        "FSTAB=!yes\n" ;
    TEST_ASSERT(environ_trim(&stk, conf), "trim boot@system") ;
    char const *exp[] = {
        "HOSTNAME=obarunS6",
        "TZ=GMT",
        "TTY=!2",       /* '!' kept verbatim */
        "FSTAB=!yes",   /* #FONT_MAP= was a comment -> absent */
    } ;
    check_entries(&stk, exp, 4) ;
    TEST_END() ;
}

/* Syntax errors drive the state machine into its error state (state 15), the
 * only path through "if (state > 14)" -> log_warn_return(LOG_EXIT_ZERO) -> 0.
 * Every other return is a success (1) or an OOM die, so a 0 here pins exactly
 * that error branch. Mutating it (e.g. dropping the "if (state > 14)" guard or
 * forcing "return 1" there) makes these cases succeed and breaks this test. */
static void test_syntax_error(void)
{
    TEST_START("test_syntax_error") ;

    /* unterminated double quote: EOF reached while still inside the quote. */
    {
        _alloc_strbuf_(stk, 64) ;
        TEST_ASSERT(environ_trim(&stk, "K=\"abc") == 0, "unclosed quote is a syntax error") ;
    }
    /* a bare "\x" escape with no following hex digit (state expects \xNN). */
    {
        _alloc_strbuf_(stk, 64) ;
        TEST_ASSERT(environ_trim(&stk, "K=\\x") == 0, "incomplete \\x escape is a syntax error") ;
    }
    /* \x followed by non-hex characters inside quotes. */
    {
        _alloc_strbuf_(stk, 64) ;
        TEST_ASSERT(environ_trim(&stk, "K=\"a\\xZZ\"") == 0, "\\x with non-hex digits is a syntax error") ;
    }
    /* a trailing lone backslash at EOF (escape started, nothing follows). */
    {
        _alloc_strbuf_(stk, 64) ;
        TEST_ASSERT(environ_trim(&stk, "K=\"abc\\") == 0, "dangling backslash is a syntax error") ;
    }

    TEST_END() ;
}

int main(void)
{
    printf("=== environ_trim test suite ===\n\n") ;

    test_basic() ;
    test_escapes() ;
    test_separator_byte_rejected() ;
    test_backslash_quote() ;
    test_hex_octal() ;
    test_whitespace() ;
    test_comments_blanks() ;
    test_empty() ;
    test_real_world() ;
    test_boot_system_shape() ;
    test_syntax_error() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
