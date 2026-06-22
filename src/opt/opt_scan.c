/*
 * opt_scan.c
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

#include <string.h>

#include <oblibs/opt.h>

static int opt_match_long(opt_t const *opts, size_t n, char const *name, size_t namelen)
{
    for (size_t i = 0 ; i < n ; i++)
        if (opts[i].longname && strlen(opts[i].longname) == namelen && !memcmp(opts[i].longname, name, namelen))
            return (int)i ;
    return -1 ;
}

static int opt_match_short(opt_t const *opts, size_t n, char c)
{
    for (size_t i = 0 ; i < n ; i++)
        if (opts[i].shortname && opts[i].shortname == c)
            return (int)i ;
    return -1 ;
}

int opt_scan(int argc, char const *const *argv, opt_t const *opts, size_t n, opt_scan_t *st)
{
    st->arg = 0 ;
    st->id = 0 ;

    /* Done with the chars of the current short cluster? advance to next token. */
    if (st->pos && (st->ind >= argc || !argv[st->ind] || !argv[st->ind][st->pos])) {
        st->ind++ ;
        st->pos = 0 ;
    }

    if (st->ind >= argc || !argv[st->ind])
        return OPT_END ;

    if (!st->pos) {

        char const *tok = argv[st->ind] ;

        if (tok[0] != '-' || !tok[1])
            return OPT_END ;                 /* positional, or lone "-" */

        if (tok[1] == '-') {

            if (!tok[2]) {                   /* "--" terminator */
                st->ind++ ;
                return OPT_END ;
            }

            /* long option: "--name" or "--name=value" */
            char const *name = tok + 2 ;
            char const *eq = strchr(name, '=') ;
            size_t namelen = eq ? (size_t)(eq - name) : strlen(name) ;
            int i = opt_match_long(opts, n, name, namelen) ;

            if (i < 0) {
                st->problem = tok ;
                st->ind++ ;
                return OPT_UNKNOWN ;
            }

            st->id = opts[i].id ;

            if (opts[i].arg == OPT_REQUIRED) {
                if (eq) {
                    st->arg = eq + 1 ;
                    st->ind++ ;
                } else {
                    if (st->ind + 1 >= argc || !argv[st->ind + 1]) {
                        st->problem = tok ;
                        st->ind++ ;
                        return OPT_MISSARG ;
                    }
                    st->arg = argv[st->ind + 1] ;
                    st->ind += 2 ;
                }
            } else {
                if (eq) {                    /* "--flag=value" but flag takes no arg */
                    st->problem = tok ;
                    st->ind++ ;
                    return OPT_UNKNOWN ;
                }
                st->ind++ ;
            }

            return st->id ;
        }

        st->pos = 1 ;                        /* short option(s): skip the leading '-' */
    }

    /* short option at argv[st->ind][st->pos] */
    {
        char const *tok = argv[st->ind] ;
        char c = tok[st->pos] ;
        int i = opt_match_short(opts, n, c) ;

        if (i < 0) {
            st->problem = tok ;
            st->ind++ ;
            st->pos = 0 ;
            return OPT_UNKNOWN ;
        }

        st->id = opts[i].id ;
        st->pos++ ;

        if (opts[i].arg == OPT_REQUIRED) {
            if (tok[st->pos]) {              /* glued: the rest of the token is the arg */
                st->arg = tok + st->pos ;
                st->ind++ ;
                st->pos = 0 ;
            } else {                         /* separate: the next token is the arg */
                if (st->ind + 1 >= argc || !argv[st->ind + 1]) {
                    st->problem = tok ;
                    st->ind++ ;
                    st->pos = 0 ;
                    return OPT_MISSARG ;
                }
                st->arg = argv[st->ind + 1] ;
                st->ind += 2 ;
                st->pos = 0 ;
            }
        }
        /* OPT_NONE: stay on the token so the next call continues the cluster. */

        return st->id ;
    }
}
