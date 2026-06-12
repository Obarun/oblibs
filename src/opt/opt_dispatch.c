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
#include <oblibs/log.h>

/* @prog is the displayed program path for THIS level: the root name at the top,
 * then "<parent path> <child name>" as we descend, so a sub-command's help reads
 * "usage: 66 tree create ...", not "usage: create ...". */
static int dispatch(int argc, char const *const *argv, opt_cmd_t const *cmd, void *data, char const *prog)
{
    opt_scan_t st = OPT_SCAN_ZERO ;

    for (;;) {

        int o = opt_scan(argc, argv, cmd->opts, cmd->nopts, &st) ;

        if (o == OPT_END)
            break ;

        if (o == OPT_UNKNOWN || o == OPT_MISSARG)
            return opt_emit_error(prog, cmd, o, &st) ;
        if (o == OPT_ID_HELP)
            return opt_emit_help(prog, cmd) ;

        if (cmd->on_option) {
            int r = cmd->on_option(o, st.arg, data) ;
            if (r)
                return r ;
        }
    }

    argc -= st.ind ;
    argv += st.ind ;

    if (cmd->sub && cmd->nsub) {

        /* Bare router node (no sub-command given): a usage error (no command), but
         * show the FULL help -- the commands: list is exactly what the user needs
         * here -- on stdout, and return LOG_EXIT_USER so scripts see the error.
         * opt_emit_help returns 0 on success / LOG_EXIT_SYS on OOM; remap the
         * success code. An *invalid* name below still gets the diagnostic + usage. */
        if (!argc || !argv[0]) {
            int r = opt_emit_help(prog, cmd) ;
            return r ? r : LOG_EXIT_USER ;
        }

        for (size_t i = 0 ; i < cmd->nsub ; i++)
            if (!strcmp(argv[0], cmd->sub[i].name)) {
                /* extend the path with the child name; the strbuf stays alive on
                 * this frame for the whole recursive sub-call below. */
                _alloc_strbuf_(child, 128) ;
                if (!strbuf_cats(&child, prog) || !strbuf_cats(&child, " ")
                 || !strbuf_cats(&child, cmd->sub[i].name) || !strbuf_terminate(&child))
                    return LOG_EXIT_SYS ;
                return dispatch(argc, argv, &cmd->sub[i], data, child.s) ;
            }

        log_fatal("invalid command: ", argv[0]) ;
        return opt_emit_usage(prog, cmd) ;
    }

    if (cmd->fn)
        return cmd->fn(argc, argv, data) ;

    return 0 ;
}

int opt_dispatch(int argc, char const *const *argv, opt_cmd_t const *cmd, void *data)
{
    return dispatch(argc, argv, cmd, data, cmd->name) ;
}
