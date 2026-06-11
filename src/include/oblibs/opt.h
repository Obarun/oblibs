/*
 * opt.h
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
 *
 * opt -- native command-line argument parsing for oblibs. Three independent
 * layers:
 *
 *   1. opt_scan       -- a unified short+long option scanner over argv[]. One
 *                        declarative table describes every option (short form,
 *                        long form, whether it takes an argument, and its help
 *                        text). Named return codes instead of magic '?'/':'.
 *                        Never exits the process; the caller decides.
 *   2. opt_dispatch   -- declarative (sub)command routing built on a tree of
 *                        opt_cmd_t. Replaces hand-rolled strcmp cascades and
 *                        argv reconstruction.
 *   3. opt_fields     -- parsing of structured option VALUES such as
 *                        "depends=a,b:groups=admin:noseed", built on the lexer.
 *
 * Layer 1 works on the shell-tokenised argv[] and is driven by the option
 * table; it is NOT a lexer job. Layer 3 works INSIDE a single argv[] element,
 * where there is a real string with real delimiters -- that IS a lexer job.
 */

#ifndef OB_OPT_H
#define OB_OPT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <oblibs/strbuf.h>

/** Number of elements of a statically-sized option/command table. */
#define OPT_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* ==========================================================================
 * Layer 1 -- option scanner
 * ========================================================================== */

/** Whether an option takes an argument. OPT_OPTIONAL is deliberately absent:
 *  an optional argument is ambiguous ("-t foo": is foo the arg or a positional?). */
enum opt_arg_e {
    OPT_NONE = 0,   /* boolean flag, no argument */
    OPT_REQUIRED,   /* requires an argument (glued "-tfoo"/"--t=foo" or next "-t foo") */
} ;

/** One option, described once for both scanning and help generation. */
typedef struct opt_s opt_t ;
struct opt_s {
    int          id ;        /* value returned by opt_scan on match (often the short char) */
    char         shortname ; /* e.g. 'h' ; 0 = no short form */
    char const  *longname ;  /* e.g. "help" ; 0 = no long form */
    bool         arg ;       /* OPT_NONE (false) | OPT_REQUIRED (true) */
    char const  *argname ;   /* e.g. "TREE" -- shown in help ; 0 when OPT_NONE */
    char const  *help ;      /* one-line description -- shown in help */
} ;

/** Re-entrant scanner state. Initialise with OPT_SCAN_ZERO before the loop. */
typedef struct opt_scan_s opt_scan_t ;
struct opt_scan_s {
    int          ind ;     /* index of the current token; on OPT_END = first positional */
    unsigned int pos ;     /* offset inside the current token (short clustering) */
    char const  *arg ;     /* argument of the option just returned (0 if OPT_NONE) */
    int          id ;      /* id of the option just returned */
    char const  *problem ; /* offending token, set on OPT_UNKNOWN / OPT_MISSARG */
} ;
#define OPT_SCAN_ZERO { .ind = 1, .pos = 0, .arg = 0, .id = 0, .problem = 0 }

/** opt_scan return codes (besides the matched option's id, which is >= 0). */
enum {
    OPT_END     = -1, /* no more options; st->ind points at the first positional */
    OPT_UNKNOWN = -2, /* unknown option; st->problem set */
    OPT_MISSARG = -3, /* option needs an argument but none was given; st->problem set */
} ;

/**
 * @brief Return the next option from argv (re-entrant; one option per call).
 *
 * argv[0] is skipped (program/command name). Stops at the first positional
 * argument (POSIX, no GNU permutation): global options must precede a
 * subcommand. Handles short clustering (-zt), glued/separate arguments
 * (-tfoo / -t foo), long options (--tree foo / --tree=foo / --color), and the
 * "--" terminator. A lone "-" is treated as a positional (returns OPT_END).
 *
 * @return >= 0 : opts[i].id of the matched option (st->arg set if OPT_REQUIRED).
 *         OPT_END / OPT_UNKNOWN / OPT_MISSARG otherwise.
 */
extern int opt_scan(int argc, char const *const *argv, opt_t const *opts, size_t n, opt_scan_t *st) ;

/* ==========================================================================
 * Layer 2 -- (sub)command dispatch
 * ========================================================================== */

/** Reserved option id: when opt_scan returns it, opt_dispatch prints the auto
 *  help and stops. Assign it to your -h/--help option. Kept out of the ASCII
 *  range so it never collides with a char-valued id. */
#define OPT_ID_HELP 256

/** Terminal command handler. This level's options have already been applied via
 *  on_option; argv is recalibrated to the FIRST POSITIONAL (argv[0] is the first
 *  non-option argument, argc its count). No argv reconstruction is needed. */
typedef int opt_cmd_fn(int argc, char const *const *argv, void *data) ;
/** Per-level option applier, called by opt_dispatch for each recognised option
 *  (except OPT_ID_HELP). Return 0 to continue; any non-zero value stops the
 *  dispatch and is returned as-is (lets the applier exit with its own code). */
typedef int opt_on_option_fn(int id, char const *arg, void *data) ;

/** One command node. A table of these describes one dispatch level; a node may
 *  point to a sub-table (sub/nsub) for nested levels (e.g. "tree" -> "create"). */
typedef struct opt_cmd_s opt_cmd_t ;
struct opt_cmd_s {
    char const         *name ;       /* "tree", "create" */
    char const         *help ;       /* one-line description for the parent's help */
    char const         *operands ;   /* positional operands shown in usage, e.g.
                                      * "service..." ; 0 = none */
    opt_t const        *opts ;       /* options recognised at this level (may be 0) */
    size_t              nopts ;
    opt_on_option_fn   *on_option ;  /* applies an option of this level (may be 0) */
    opt_cmd_t const    *sub ;        /* sub-commands (recursion) ; 0 if leaf */
    size_t              nsub ;
    opt_cmd_fn         *fn ;         /* terminal handler ; 0 if pure routing node */
} ;

/**
 * @brief Scan this level's options, then route to a sub-command or call fn.
 *
 * Scans @cmd->opts with opt_scan; each recognised option is passed to
 * @cmd->on_option. -h/--help is intercepted and prints the auto help.
 * At the first positional, argv is recalibrated: if @cmd->sub is set the
 * positional selects a sub-command and dispatch recurses, otherwise @cmd->fn
 * is called. @data is the application context (e.g. ssexec_t *).
 *
 * @return the handler's return value, or an error exit code on bad usage.
 */
extern int opt_dispatch(int argc, char const *const *argv, opt_cmd_t const *cmd, void *data) ;

/* ---- help generation (built from the tables above) ------------------------*/

/** @brief Build into @out the one-line "prog [ -x ] ... command ..." usage for
 *  command node @cmd. @return 1 / 0 (errno). */
extern uint8_t opt_usage(strbuf *out, char const *prog, opt_cmd_t const *cmd) ;

/** @brief Build into @out the full help block (usage + one line per option and
 *  per sub-command) for @cmd. @return 1 / 0 (errno). */
extern uint8_t opt_help(strbuf *out, char const *prog, opt_cmd_t const *cmd) ;

/** @brief Build the full help block for @cmd and write it to stdout. Wrapper
 *  over opt_help for the common "print help and exit 0" path.
 *  @return 0 on success, LOG_EXIT_SYS if the build failed. */
extern int opt_emit_help(opt_cmd_t const *cmd) ;

/** @brief Build the one-line usage for @cmd and write it to stderr. Wrapper
 *  over opt_usage for the common error-path "print usage and bail" case.
 *  @return LOG_EXIT_USER (always -- usage is an error-path output). */
extern int opt_emit_usage(opt_cmd_t const *cmd) ;

/** @brief Report an opt_scan failure: log the offending token from @st->problem
 *  (@code selects the message: OPT_UNKNOWN vs OPT_MISSARG), then emit the usage.
 *  Lets a manual opt_scan loop handle both error codes in one line.
 *  @return LOG_EXIT_USER. */
extern int opt_emit_error(opt_cmd_t const *cmd, int code, opt_scan_t const *st) ;

/* ==========================================================================
 * Layer 3 -- structured option values ("k=v:k=v,v"), built on the lexer
 * ========================================================================== */

/** One recognised field of a structured "-o" value. */
typedef struct opt_field_s opt_field_t ;
struct opt_field_s {
    int          id ;      /* passed back to the callback on match */
    char const  *key ;     /* "depends" */
    bool         hasval ;  /* true = "key=val" required ; false = bare key allowed ("noseed") */
} ;

/** Called once per parsed field. @val is 0 for a bare key. Return 1 on success,
 *  0 to abort the parse (opt_fields then returns 0). */
typedef int opt_field_fn(int id, char const *key, char const *val, void *data) ;

/**
 * @brief Parse a structured value like "depends=a,b:groups=admin:noseed".
 *
 * Splits @str on @fieldsep (e.g. ':'), then each element on @kvsep (e.g. '=')
 * into key/val (val == 0 when no @kvsep). The key is matched against @fields;
 * hasval is enforced (bare key vs key=val). For each field, @fn is called.
 * Built on lexer_trim_with_delim.
 *
 * @return 1 on success, 0 on failure (errno / log on invalid input).
 */
extern uint8_t opt_fields(char const *str, char fieldsep, char kvsep,
                          opt_field_t const *fields, size_t n,
                          opt_field_fn *fn, void *data) ;

/**
 * @brief Split a list value "a,b,c" on @sep into @out as an sbl (NUL-separated).
 *
 * Helper for opt_field_fn callbacks handling list-typed fields (depends, allow,
 * ...). Built on lexer_trim_with_delim. @out is appended to.
 *
 * @return 1 on success, 0 on failure.
 */
extern uint8_t opt_list(strbuf *out, char const *val, char sep) ;

#endif
