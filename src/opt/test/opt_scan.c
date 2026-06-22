/*
 * opt_scan.c -- tests for opt_scan (layer 1)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <oblibs/opt.h>

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

static opt_t const opts[] = {
    { .id = 'h', .shortname = 'h', .longname = "help",    .arg = OPT_NONE,     .argname = 0,      .help = "print help" },
    { .id = 'z', .shortname = 'z', .longname = "color",   .arg = OPT_NONE,     .argname = 0,      .help = "use colors" },
    { .id = 't', .shortname = 't', .longname = "tree",    .arg = OPT_REQUIRED, .argname = "TREE", .help = "select tree" },
    { .id = 'v', .shortname = 'v', .longname = "verbose", .arg = OPT_REQUIRED, .argname = "N",    .help = "verbosity" },
} ;

/* argc derived from a NULL-terminated argv */
static int countargv(char const *const *argv)
{
    int n = 0 ;
    while (argv[n]) n++ ;
    return n ;
}

static void test_cluster_separate_arg(void)
{
    TEST_START("test_cluster_separate_arg") ;
    char const *argv[] = { "prog", "-zt", "foo", "bar", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'z', "first is -z") ;
    TEST_ASSERT(st.arg == 0, "z has no arg") ;

    int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
    TEST_ASSERT(o == 't', "second is -t") ;
    TEST_ASSERT(st.arg && !strcmp(st.arg, "foo"), "t arg is foo") ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == 3, "ind points at 'bar'") ;
    TEST_ASSERT(!strcmp(argv[st.ind], "bar"), "positional is bar") ;
    TEST_END() ;
}

static void test_glued_arg(void)
{
    TEST_START("test_glued_arg") ;
    char const *argv[] = { "prog", "-tfoo", "bar", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
    TEST_ASSERT(o == 't', "is -t") ;
    TEST_ASSERT(st.arg && !strcmp(st.arg, "foo"), "glued arg is foo") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == 2, "ind at 'bar'") ;
    TEST_END() ;
}

static void test_long_eq(void)
{
    TEST_START("test_long_eq") ;
    char const *argv[] = { "prog", "--tree=foo", "bar", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
    TEST_ASSERT(o == 't', "is --tree") ;
    TEST_ASSERT(st.arg && !strcmp(st.arg, "foo"), "arg is foo") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == 2, "ind at 'bar'") ;
    TEST_END() ;
}

static void test_long_separate(void)
{
    TEST_START("test_long_separate") ;
    char const *argv[] = { "prog", "--tree", "foo", "bar", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
    TEST_ASSERT(o == 't', "is --tree") ;
    TEST_ASSERT(st.arg && !strcmp(st.arg, "foo"), "arg is foo") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == 3, "ind at 'bar'") ;
    TEST_END() ;
}

static void test_long_flag(void)
{
    TEST_START("test_long_flag") ;
    char const *argv[] = { "prog", "--color", "x", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'z', "is --color") ;
    TEST_ASSERT(st.arg == 0, "no arg") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == 2, "ind at 'x'") ;
    TEST_END() ;
}

static void test_double_dash(void)
{
    TEST_START("test_double_dash") ;
    char const *argv[] = { "prog", "-z", "--", "-t", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'z', "is -z") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "-- ends options") ;
    TEST_ASSERT(st.ind == 3, "ind past '--'") ;
    TEST_ASSERT(!strcmp(argv[st.ind], "-t"), "'-t' is now positional") ;
    TEST_END() ;
}

static void test_lone_dash(void)
{
    TEST_START("test_lone_dash") ;
    char const *argv[] = { "prog", "-", "x", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "lone - is positional") ;
    TEST_ASSERT(st.ind == 1, "ind at '-'") ;
    TEST_END() ;
}

static void test_unknown_short(void)
{
    TEST_START("test_unknown_short") ;
    char const *argv[] = { "prog", "-x", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_UNKNOWN, "unknown short") ;
    TEST_ASSERT(st.problem && !strcmp(st.problem, "-x"), "problem token set") ;
    TEST_END() ;
}

static void test_unknown_long(void)
{
    TEST_START("test_unknown_long") ;
    char const *argv[] = { "prog", "--bogus", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_UNKNOWN, "unknown long") ;
    TEST_ASSERT(st.problem && !strcmp(st.problem, "--bogus"), "problem token set") ;
    TEST_END() ;
}

static void test_missarg_short(void)
{
    TEST_START("test_missarg_short") ;
    char const *argv[] = { "prog", "-t", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_MISSARG, "missing arg") ;
    TEST_END() ;
}

static void test_missarg_long(void)
{
    TEST_START("test_missarg_long") ;
    char const *argv[] = { "prog", "--tree", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_MISSARG, "missing arg") ;
    TEST_END() ;
}

static void test_flag_with_eq(void)
{
    TEST_START("test_flag_with_eq") ;
    char const *argv[] = { "prog", "--color=x", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_UNKNOWN, "flag takes no arg") ;
    TEST_END() ;
}

static void test_all_bool_cluster(void)
{
    TEST_START("test_all_bool_cluster") ;
    char const *argv[] = { "prog", "-hz", "pos", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'h', "first h") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'z', "then z") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == 2, "ind at 'pos'") ;
    TEST_END() ;
}

/* A bool cluster that ends argv (no positional after it) must still terminate
 * cleanly with ind == argc -- exercises the end-of-argv boundary. */
static void test_bool_cluster_last(void)
{
    TEST_START("test_bool_cluster_last") ;
    char const *argv[] = { "prog", "-hz", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'h', "first h") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == 'z', "then z") ;
    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "then end") ;
    TEST_ASSERT(st.ind == argc, "ind at end of argv") ;
    TEST_END() ;
}

static void test_no_options(void)
{
    TEST_START("test_no_options") ;
    char const *argv[] = { "prog", "cmd", "arg", 0 } ;
    int argc = countargv(argv) ;
    opt_scan_t st = OPT_SCAN_ZERO ;

    TEST_ASSERT(opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) == OPT_END, "immediate end") ;
    TEST_ASSERT(st.ind == 1, "ind at 'cmd'") ;
    TEST_END() ;
}

int main(void)
{
    test_cluster_separate_arg() ;
    test_glued_arg() ;
    test_long_eq() ;
    test_long_separate() ;
    test_long_flag() ;
    test_double_dash() ;
    test_lone_dash() ;
    test_unknown_short() ;
    test_unknown_long() ;
    test_missarg_short() ;
    test_missarg_long() ;
    test_flag_with_eq() ;
    test_all_bool_cluster() ;
    test_bool_cluster_last() ;
    test_no_options() ;
    printf("\nAll opt_scan tests passed.\n") ;
    return 0 ;
}
