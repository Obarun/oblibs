/*
 * opt_dispatch.c
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
 * Layer 2: declarative (sub)command dispatch. Replaces the hand-rolled strcmp
 * cascades and the fragile nargv[] reconstruction: each level scans its own
 * options with opt_scan, then either routes to a sub-command or calls a leaf
 * handler on the recalibrated argv.
 */

#include <string.h>

#include <oblibs/opt.h>
#include <oblibs/strbuf.h>
#include <oblibs/io.h>
#include <oblibs/log.h>

static int opt_emit_help(opt_cmd_t const *cmd)
{
    _alloc_strbuf_(sb, 512) ;
    if (!opt_help(&sb, cmd->name, cmd) || !strbuf_cats(&sb, "\n"))
        return LOG_EXIT_SYS ;
    io_allwrite(1, sb.s, sb.len) ;
    return 0 ;
}

static int opt_emit_usage(opt_cmd_t const *cmd)
{
    _alloc_strbuf_(sb, 256) ;
    if (opt_usage(&sb, cmd->name, cmd) && strbuf_cats(&sb, "\n"))
        io_allwrite(2, sb.s, sb.len) ;
    return LOG_EXIT_USER ;
}

int opt_dispatch(int argc, char const *const *argv, opt_cmd_t const *cmd, void *data)
{
    opt_scan_t st = OPT_SCAN_ZERO ;

    for (;;) {

        int o = opt_scan(argc, argv, cmd->opts, cmd->nopts, &st) ;

        if (o == OPT_END)
            break ;

        if (o == OPT_UNKNOWN) {
            log_warn("invalid option: ", st.problem) ;
            return opt_emit_usage(cmd) ;
        }
        if (o == OPT_MISSARG) {
            log_warn("option requires an argument: ", st.problem) ;
            return opt_emit_usage(cmd) ;
        }
        if (o == OPT_ID_HELP)
            return opt_emit_help(cmd) ;

        if (cmd->on_option) {
            int r = cmd->on_option(o, st.arg, data) ;
            if (r)
                return r ;
        }
    }

    argc -= st.ind ;
    argv += st.ind ;

    if (cmd->sub && cmd->nsub) {

        if (!argc || !argv[0]) {
            log_warn("missing command") ;
            return opt_emit_usage(cmd) ;
        }

        for (size_t i = 0 ; i < cmd->nsub ; i++)
            if (!strcmp(argv[0], cmd->sub[i].name))
                return opt_dispatch(argc, argv, &cmd->sub[i], data) ;

        log_warn("invalid command: ", argv[0]) ;
        return opt_emit_usage(cmd) ;
    }

    if (cmd->fn)
        return cmd->fn(argc, argv, data) ;

    return 0 ;
}
