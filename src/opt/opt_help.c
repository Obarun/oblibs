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
 * except according to the terms contained in the LICENSE file./
 *
 * Help generation: usage line and full help block built from the opt_t /
 * opt_cmd_t tables, so the option declaration is the single source of truth.
 */

#include <string.h>

#include <oblibs/opt.h>
#include <oblibs/strbuf.h>

#define OPT_HELP_COL 24  /* left-column width before the description text */

/* Build the left column of one option ("-t, --tree TREE") into @col,
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
    if (o->arg == OPT_REQUIRED && o->argname)
        if (!strbuf_cats(col, " ") || !strbuf_cats(col, o->argname))
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

    if (!strbuf_terminate(out))
        return 0 ;
    out->len-- ;
    return 1 ;
}

uint8_t opt_help(strbuf *out, char const *prog, opt_cmd_t const *cmd)
{
    if (!opt_usage(out, prog, cmd) || !strbuf_cats(out, "\n"))
        return 0 ;

    if (cmd->help && *cmd->help)
        if (!strbuf_cats(out, "\n") || !strbuf_cats(out, cmd->help) || !strbuf_cats(out, "\n"))
            return 0 ;

    if (cmd->nopts && cmd->opts) {

        if (!strbuf_cats(out, "\noptions:\n"))
            return 0 ;

        for (size_t i = 0 ; i < cmd->nopts ; i++) {

            _alloc_strbuf_(col, 64) ;
            if (!opt_left_col(&col, &cmd->opts[i]))
                return 0 ;
            size_t printable = col.len ? col.len - 1 : 0 ;

            if (!strbuf_cats(out, "    ") || !strbuf_cats(out, col.s))
                return 0 ;
            if (!opt_pad(out, 4 + printable, OPT_HELP_COL))
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

            if (!strbuf_cats(out, "    ") || !strbuf_cats(out, cmd->sub[i].name))
                return 0 ;
            if (!opt_pad(out, 4 + strlen(cmd->sub[i].name), OPT_HELP_COL))
                return 0 ;
            if (cmd->sub[i].help)
                if (!strbuf_cats(out, cmd->sub[i].help))
                    return 0 ;
            if (!strbuf_cats(out, "\n"))
                return 0 ;
        }
    }

    if (!strbuf_terminate(out))
        return 0 ;
    out->len-- ;
    return 1 ;
}
