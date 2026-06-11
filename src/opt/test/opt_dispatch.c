/*
 * opt_dispatch.c -- tests for opt_dispatch (layer 2)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oblibs/opt.h>
#include <oblibs/log.h>

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

/* shared recording context */
typedef struct ctx_s ctx_t ;
struct ctx_s {
    char tree[64] ;   /* set by the global -t option */
    char cmd[64] ;    /* set by the leaf handler */
    char pos[64] ;    /* first positional seen by the leaf handler */
    int  leaf_argc ;
} ;

static int on_global(int id, char const *arg, void *data)
{
    ctx_t *c = data ;
    if (id == 't')
        strcpy(c->tree, arg) ;
    return 0 ;
}

static int do_create(int argc, char const *const *argv, void *data)
{
    ctx_t *c = data ;
    strcpy(c->cmd, "create") ;
    c->leaf_argc = argc ;
    if (argc > 0)
        strcpy(c->pos, argv[0]) ;
    return 0 ;
}

static int do_enable(int argc, char const *const *argv, void *data)
{
    ctx_t *c = data ;
    strcpy(c->cmd, "enable") ;
    c->leaf_argc = argc ;
    if (argc > 0)
        strcpy(c->pos, argv[0]) ;
    return 0 ;
}

static opt_t const hopt[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_t const gopts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE,     .help = "print this help" },
    { .id = 't',         .shortname = 't', .longname = "tree", .arg = OPT_REQUIRED, .argname = "TREE", .help = "select tree" },
} ;

static opt_cmd_t const tree_sub[] = {
    { .name = "create", .help = "create a tree", .opts = hopt, .nopts = OPT_COUNT(hopt), .fn = &do_create },
    { .name = "enable", .help = "enable a tree", .opts = hopt, .nopts = OPT_COUNT(hopt), .fn = &do_enable },
} ;

static opt_cmd_t const root_sub[] = {
    { .name = "tree", .help = "tree management", .opts = hopt, .nopts = OPT_COUNT(hopt), .sub = tree_sub, .nsub = OPT_COUNT(tree_sub) },
} ;

static opt_cmd_t const root = {
    .name = "66", .help = "service manager",
    .opts = gopts, .nopts = OPT_COUNT(gopts), .on_option = &on_global,
    .sub = root_sub, .nsub = OPT_COUNT(root_sub),
} ;

static int countargv(char const *const *argv)
{
    int n = 0 ;
    while (argv[n]) n++ ;
    return n ;
}

static void test_nested_route(void)
{
    TEST_START("test_nested_route") ;
    char const *argv[] = { "66", "-t", "foo", "tree", "create", "mytree", 0 } ;
    ctx_t c = { .tree = "", .cmd = "", .pos = "", .leaf_argc = 0 } ;

    int r = opt_dispatch(countargv(argv), argv, &root, &c) ;
    TEST_ASSERT(r == 0, "dispatch ok") ;
    TEST_ASSERT(!strcmp(c.tree, "foo"), "global -t applied") ;
    TEST_ASSERT(!strcmp(c.cmd, "create"), "routed to create") ;
    TEST_ASSERT(!strcmp(c.pos, "mytree"), "positional reached leaf") ;
    TEST_ASSERT(c.leaf_argc == 1, "leaf sees only the positional") ;
    TEST_END() ;
}

static void test_route_enable(void)
{
    TEST_START("test_route_enable") ;
    char const *argv[] = { "66", "tree", "enable", "bar", 0 } ;
    ctx_t c = { .tree = "", .cmd = "", .pos = "", .leaf_argc = 0 } ;

    int r = opt_dispatch(countargv(argv), argv, &root, &c) ;
    TEST_ASSERT(r == 0, "dispatch ok") ;
    TEST_ASSERT(c.tree[0] == 0, "no global option") ;
    TEST_ASSERT(!strcmp(c.cmd, "enable"), "routed to enable") ;
    TEST_ASSERT(!strcmp(c.pos, "bar"), "positional bar") ;
    TEST_END() ;
}

static void test_unknown_command(void)
{
    TEST_START("test_unknown_command") ;
    char const *argv[] = { "66", "bogus", 0 } ;
    ctx_t c = { .tree = "", .cmd = "", .pos = "", .leaf_argc = 0 } ;

    fprintf(stderr, "  (expect a diagnostic below)\n") ;
    int r = opt_dispatch(countargv(argv), argv, &root, &c) ;
    TEST_ASSERT(r == LOG_EXIT_USER, "unknown command -> usage error") ;
    TEST_ASSERT(c.cmd[0] == 0, "no leaf called") ;
    TEST_END() ;
}

static void test_unknown_subcommand(void)
{
    TEST_START("test_unknown_subcommand") ;
    char const *argv[] = { "66", "tree", "bogus", 0 } ;
    ctx_t c = { .tree = "", .cmd = "", .pos = "", .leaf_argc = 0 } ;

    fprintf(stderr, "  (expect a diagnostic below)\n") ;
    int r = opt_dispatch(countargv(argv), argv, &root, &c) ;
    TEST_ASSERT(r == LOG_EXIT_USER, "unknown subcommand -> usage error") ;
    TEST_END() ;
}

static void test_help_root(void)
{
    TEST_START("test_help_root") ;
    char const *argv[] = { "66", "-h", 0 } ;
    ctx_t c = { .tree = "", .cmd = "", .pos = "", .leaf_argc = 0 } ;

    printf("\n--- help output below ---\n") ;
    int r = opt_dispatch(countargv(argv), argv, &root, &c) ;
    printf("--- end help output ---\n") ;
    TEST_ASSERT(r == 0, "help returns 0") ;
    TEST_ASSERT(c.cmd[0] == 0, "no leaf called") ;
    TEST_END() ;
}

/* opt_usage appends cmd->operands last (after "command [ args ]" when present) */
static void test_usage_operands(void)
{
    TEST_START("test_usage_operands") ;

    static opt_t const fopt[] = {
        { .id = 'f', .shortname = 'f', .longname = "field", .arg = OPT_REQUIRED, .argname = "FIELD", .help = "a field" },
    } ;
    static opt_cmd_t const leaf = {
        .name = "read", .help = "read it", .operands = "service...",
        .opts = fopt, .nopts = OPT_COUNT(fopt), .fn = &do_create,
    } ;
    static opt_cmd_t const rnode = {
        .name = "66", .help = "x", .operands = "service...|tree",
        .opts = hopt, .nopts = OPT_COUNT(hopt), .sub = tree_sub, .nsub = OPT_COUNT(tree_sub),
    } ;

    _alloc_strbuf_(sb, 128) ;
    TEST_ASSERT(opt_usage(&sb, "read", &leaf), "opt_usage (leaf) ok") ;
    TEST_ASSERT(!strcmp(sb.s, "usage: read [ -f FIELD ] service..."),
                "operands appended on a leaf") ;

    _alloc_strbuf_(sb2, 128) ;
    TEST_ASSERT(opt_usage(&sb2, "66", &rnode), "opt_usage (routing) ok") ;
    TEST_ASSERT(!strcmp(sb2.s, "usage: 66 [ -h ] command [ args ] service...|tree"),
                "operands appended after 'command [ args ]'") ;

    TEST_END() ;
}

/* opt_help lists a sub-command's operands in the "commands:" section */
static void test_help_operands(void)
{
    TEST_START("test_help_operands") ;

    static opt_cmd_t const leaves[] = {
        { .name = "read", .help = "read it", .operands = "service",
          .opts = hopt, .nopts = OPT_COUNT(hopt), .fn = &do_create },
    } ;
    static opt_cmd_t const r = {
        .name = "tool", .help = "x",
        .opts = hopt, .nopts = OPT_COUNT(hopt),
        .sub = leaves, .nsub = OPT_COUNT(leaves),
    } ;

    _alloc_strbuf_(h, 256) ;
    TEST_ASSERT(opt_help(&h, "tool", &r), "opt_help ok") ;
    TEST_ASSERT(strstr(h.s, "read service") != 0, "sub-command operands in commands section") ;

    TEST_END() ;
}

/* a .hidden option is omitted from usage/help but still parsable by opt_scan */
static void test_hidden(void)
{
    TEST_START("test_hidden") ;

    static opt_t const opts[] = {
        { .id = 'P', .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "internal", .hidden = true },
        { .id = 's', .shortname = 's', .longname = "sep",          .arg = OPT_REQUIRED, .argname = "SEP", .help = "separator" },
    } ;
    static opt_cmd_t const leaf = {
        .name = "enable", .help = "enable it", .operands = "service...",
        .opts = opts, .nopts = OPT_COUNT(opts), .fn = &do_create,
    } ;

    /* usage: the hidden -P is gone, the visible -s stays */
    _alloc_strbuf_(u, 128) ;
    TEST_ASSERT(opt_usage(&u, "enable", &leaf), "opt_usage ok") ;
    TEST_ASSERT(!strcmp(u.s, "usage: enable [ -s SEP ] service..."),
                "hidden option absent from usage") ;

    /* help: -P not listed, -s listed */
    _alloc_strbuf_(h, 256) ;
    TEST_ASSERT(opt_help(&h, "enable", &leaf), "opt_help ok") ;
    TEST_ASSERT(strstr(h.s, "no-propagate") == 0 && strstr(h.s, "-P") == 0,
                "hidden option absent from help") ;
    TEST_ASSERT(strstr(h.s, "-s, --sep SEP") != 0, "visible option present in help") ;

    /* but opt_scan still matches the hidden option (hidden != unparsable) */
    char const *argv[] = { "enable", "-P", "svc", 0 } ;
    opt_scan_t st = OPT_SCAN_ZERO ;
    int o = opt_scan(countargv(argv), argv, opts, OPT_COUNT(opts), &st) ;
    TEST_ASSERT(o == 'P', "hidden option still parsed by opt_scan") ;

    TEST_END() ;
}

/* a table whose every option is hidden produces no "options:" section */
static void test_all_hidden(void)
{
    TEST_START("test_all_hidden") ;

    static opt_t const opts[] = {
        { .id = 'x', .shortname = 'x', .arg = OPT_NONE, .help = "internal", .hidden = true },
    } ;
    static opt_cmd_t const leaf = {
        .name = "free", .help = "free it", .opts = opts, .nopts = OPT_COUNT(opts), .fn = &do_create,
    } ;

    _alloc_strbuf_(h, 128) ;
    TEST_ASSERT(opt_help(&h, "free", &leaf), "opt_help ok") ;
    TEST_ASSERT(strstr(h.s, "options:") == 0, "no empty options section when all hidden") ;

    _alloc_strbuf_(u, 128) ;
    TEST_ASSERT(opt_usage(&u, "free", &leaf), "opt_usage ok") ;
    TEST_ASSERT(!strcmp(u.s, "usage: free"), "usage has no brackets when all hidden") ;

    TEST_END() ;
}

/* cmd->epilog is appended to opt_help (after the sections) but never to opt_usage */
static void test_epilog(void)
{
    TEST_START("test_epilog") ;

    static opt_t const opts[] = {
        { .id = 'w', .shortname = 'w', .arg = OPT_REQUIRED, .argname = "STATE", .help = "wait for STATE" },
    } ;
    static opt_cmd_t const leaf = {
        .name = "signal", .help = "send a signal", .operands = "service...",
        .opts = opts, .nopts = OPT_COUNT(opts), .fn = &do_create,
        .epilog =
            "values for -w (wait until the service reaches the state):\n"
            "    u  service is up\n"
            "    D  service is down and ready to be brought up",
    } ;

    /* help: the epilog text is present, after the options: section */
    _alloc_strbuf_(h, 512) ;
    TEST_ASSERT(opt_help(&h, "signal", &leaf), "opt_help ok") ;
    char const *opts_at = strstr(h.s, "options:") ;
    char const *epi_at  = strstr(h.s, "values for -w") ;
    TEST_ASSERT(epi_at != 0, "epilog present in help") ;
    TEST_ASSERT(strstr(h.s, "D  service is down") != 0, "epilog body present") ;
    TEST_ASSERT(opts_at != 0 && epi_at > opts_at, "epilog comes after the options section") ;

    /* usage: the epilog must NOT leak into the one-line synopsis */
    _alloc_strbuf_(u, 128) ;
    TEST_ASSERT(opt_usage(&u, "signal", &leaf), "opt_usage ok") ;
    TEST_ASSERT(strstr(u.s, "values for -w") == 0, "epilog absent from usage") ;
    TEST_ASSERT(!strcmp(u.s, "usage: signal [ -w STATE ] service..."), "usage unaffected") ;

    TEST_END() ;
}

int main(void)
{
    test_nested_route() ;
    test_route_enable() ;
    test_unknown_command() ;
    test_unknown_subcommand() ;
    test_help_root() ;
    test_usage_operands() ;
    test_help_operands() ;
    test_hidden() ;
    test_all_hidden() ;
    test_epilog() ;
    printf("\nAll opt_dispatch tests passed.\n") ;
    return 0 ;
}
