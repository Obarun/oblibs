/*
 * opt_help.c
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
 * Help generation: usage line and full help block built from the opt_t /
 * opt_cmd_t tables, so the option declaration is the single source of truth.
 */

#include <oblibs/opt.h>
#include <oblibs/strbuf.h>
#include <oblibs/io.h>
#include <oblibs/log.h>

#define OPT_HELP_INDENT 4  /* leading spaces before each entry */
#define OPT_HELP_GAP    2  /* spaces between the left column and the description */

/* Build the left column of one option ("-t, --tree=TREE") into @col,
 * NUL-terminated; @col->len then includes the trailing NUL. */
static uint8_t opt_left_col(strbuf *col, opt_t const *o)
{
    col->len = 0 ;

    if (o->shortname) {
        char s[3] = { '-', o->shortname, 0 } ;
        if (!strbuf_cats(col, s))
            return 0 ;
        if (o->longname)
            if (!strbuf_cats(col, ", "))
                return 0 ;
    }
    if (o->longname)
        if (!strbuf_cats(col, "--") || !strbuf_cats(col, o->longname))
            return 0 ;
    if (o->arg == OPT_REQUIRED && o->argname) {
        /* Glue the arg to the long form with '=' ("--tree=TREE"): easier to read in
         * the list, and "--tree TREE" stays valid too. A short-only option keeps a
         * space ("-o K=V") -- '=' is a long-option convention, not a short one. */
        if (!strbuf_cats(col, o->longname ? "=" : " ") || !strbuf_cats(col, o->argname))
            return 0 ;
    }

    return strbuf_terminate(col) ;
}

/* Build the left column of a sub-command (just its name) into @col,
 * NUL-terminated; @col->len then includes the trailing NUL. Operands are
 * intentionally omitted here: they belong to each sub-command's own usage line
 * (opt_usage), not to the parent's commands: index. */
static uint8_t opt_sub_left_col(strbuf *col, opt_cmd_t const *s)
{
    col->len = 0 ;

    if (!strbuf_cats(col, s->name))
        return 0 ;

    return strbuf_terminate(col) ;
}

static uint8_t opt_pad(strbuf *out, size_t cur, size_t width)
{
    if (cur + 1 >= width) /* always at least one space before the text */
        return strbuf_cats(out, " ") ;
    while (cur < width) {
        if (!strbuf_cats(out, " "))
            return 0 ;
        cur++ ;
    }
    return 1 ;
}

uint8_t opt_usage(strbuf *out, char const *prog, opt_cmd_t const *cmd)
{
    if (!strbuf_cats(out, "usage: ") || !strbuf_cats(out, prog ? prog : ""))
        return 0 ;

    for (size_t i = 0 ; i < cmd->nopts ; i++) {

        opt_t const *o = &cmd->opts[i] ;
        char head[3] = { '-', o->shortname, 0 } ;

        if (o->hidden)
            continue ;

        if (!strbuf_cats(out, " [ "))
            return 0 ;
        if (o->shortname) {
            if (!strbuf_cats(out, head))
                return 0 ;
        } else if (o->longname) {
            if (!strbuf_cats(out, "--") || !strbuf_cats(out, o->longname))
                return 0 ;
        }
        if (o->arg == OPT_REQUIRED && o->argname)
            if (!strbuf_cats(out, " ") || !strbuf_cats(out, o->argname))
                return 0 ;
        if (!strbuf_cats(out, " ]"))
            return 0 ;
    }

    if (cmd->nsub && cmd->sub) {
        if (!strbuf_cats(out, " command [ args ]"))
            return 0 ;
    }

    if (cmd->operands && *cmd->operands)
        if (!strbuf_cats(out, " ") || !strbuf_cats(out, cmd->operands))
            return 0 ;

    if (!strbuf_uncounted(out))
        return 0 ;
    return 1 ;
}

uint8_t opt_help(strbuf *out, char const *prog, opt_cmd_t const *cmd)
{
    if (!opt_usage(out, prog, cmd) || !strbuf_cats(out, "\n"))
        return 0 ;

    if (cmd->help && *cmd->help)
        if (!strbuf_cats(out, "\n") || !strbuf_cats(out, cmd->help) || !strbuf_cats(out, "\n"))
            return 0 ;

    /* Pass 1: widest left column across options AND sub-commands, so every
     * description aligns even when one entry overruns a fixed column width. */
    size_t maxw = 0 ;
    for (size_t i = 0 ; i < cmd->nopts ; i++) {
        if (cmd->opts[i].hidden)
            continue ;
        _alloc_strbuf_(col, 64) ;
        if (!opt_left_col(&col, &cmd->opts[i]))
            return 0 ;
        size_t printable = col.len ? col.len - 1 : 0 ;
        if (printable > maxw) maxw = printable ;
    }
    for (size_t i = 0 ; i < cmd->nsub ; i++) {
        _alloc_strbuf_(col, 64) ;
        if (!opt_sub_left_col(&col, &cmd->sub[i]))
            return 0 ;
        size_t printable = col.len ? col.len - 1 : 0 ;
        if (printable > maxw) maxw = printable ;
    }
    size_t width = OPT_HELP_INDENT + maxw + OPT_HELP_GAP ;

    /* Pass 2: emit, aligning all descriptions on `width`. The "options:" header
     * is emitted lazily, before the first VISIBLE option, so a table whose options
     * are all hidden produces no empty section. */
    if (cmd->nopts && cmd->opts) {

        bool header_done = false ;

        for (size_t i = 0 ; i < cmd->nopts ; i++) {

            if (cmd->opts[i].hidden)
                continue ;

            if (!header_done) {
                if (!strbuf_cats(out, "\noptions:\n"))
                    return 0 ;
                header_done = true ;
            }

            _alloc_strbuf_(col, 64) ;
            if (!opt_left_col(&col, &cmd->opts[i]))
                return 0 ;
            size_t printable = col.len ? col.len - 1 : 0 ;

            if (!strbuf_cats(out, "    ") || !strbuf_cats(out, col.s))
                return 0 ;
            if (!opt_pad(out, OPT_HELP_INDENT + printable, width))
                return 0 ;
            if (cmd->opts[i].help)
                if (!strbuf_cats(out, cmd->opts[i].help))
                    return 0 ;
            if (!strbuf_cats(out, "\n"))
                return 0 ;
        }
    }

    if (cmd->nsub && cmd->sub) {

        if (!strbuf_cats(out, "\ncommands:\n"))
            return 0 ;

        for (size_t i = 0 ; i < cmd->nsub ; i++) {

            _alloc_strbuf_(col, 64) ;
            if (!opt_sub_left_col(&col, &cmd->sub[i]))
                return 0 ;
            size_t printable = col.len ? col.len - 1 : 0 ;

            if (!strbuf_cats(out, "    ") || !strbuf_cats(out, col.s))
                return 0 ;
            if (!opt_pad(out, OPT_HELP_INDENT + printable, width))
                return 0 ;
            if (cmd->sub[i].help)
                if (!strbuf_cats(out, cmd->sub[i].help))
                    return 0 ;
            if (!strbuf_cats(out, "\n"))
                return 0 ;
        }
    }

    /* Free-form epilog: for what the option table cannot model (the meaning of a
     * structured value, the fields a list accepts, ...). Same shape as cmd->help:
     * a blank line, the text verbatim, a closing newline. */
    if (cmd->epilog && *cmd->epilog)
        if (!strbuf_cats(out, "\n") || !strbuf_cats(out, cmd->epilog) || !strbuf_cats(out, "\n"))
            return 0 ;

    if (!strbuf_uncounted(out))
        return 0 ;
    return 1 ;
}

int opt_emit_help(char const *prog, opt_cmd_t const *cmd)
{
    _alloc_strbuf_(sb, 512) ;
    if (!opt_help(&sb, prog, cmd) || !strbuf_cats(&sb, "\n"))
        return LOG_EXIT_SYS ;
    io_allwrite(1, sb.s, sb.len) ;
    return 0 ;
}

int opt_emit_usage(char const *prog, opt_cmd_t const *cmd)
{
    _alloc_strbuf_(sb, 256) ;
    if (opt_usage(&sb, prog, cmd) && strbuf_cats(&sb, "\n"))
        io_allwrite(2, sb.s, sb.len) ;
    return LOG_EXIT_USER ;
}

int opt_emit_error(char const *prog, opt_cmd_t const *cmd, int code, opt_scan_t const *st)
{
    /* ERROR level (log_fatal), not WARN: a bad option is a user error and must
     * stay visible at the default verbosity (WARN is hidden at VERBOSITY=1).
     * log_fatal prints but does not exit -- the caller returns LOG_EXIT_USER. */
    if (code == OPT_MISSARG)
        log_fatal("option requires an argument: ", st->problem) ;
    else
        log_fatal("invalid option: ", st->problem) ;
    return opt_emit_usage(prog, cmd) ;
}
