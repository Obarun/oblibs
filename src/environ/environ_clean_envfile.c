/*
 * environ_clean_envfile.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/environ.h>

#include <string.h>
#include <unistd.h>

#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int environ_clean_envfile(stralloc *modifs, char const *path)
{
    int r, e = 0 ;
    size_t pos = 0 ;

    stralloc toparse = STRALLOC_ZERO ;
    stralloc again = STRALLOC_ZERO ;
    stralloc tokeep = STRALLOC_ZERO ;
    char const *exclude[1] = { 0 } ;
    _init_stack_(smodifs, modifs->len + 1) ;

    if (modifs->len) {

        if (!stack_convert_string(&smodifs, modifs->s, modifs->len))
            return e ;

        if (!stralloc_copyb(modifs, smodifs.s, modifs->len))
            return e ;
    }

    r = sastr_dir_get(&toparse,path,exclude,S_IFREG) ;

    if (!r)
        goto err ;

    /** empty path directory
     * we don't die */
    if (!toparse.len)
        goto freed ;

    if (!sastr_sort(&toparse))
        goto err ;

    if (sastr_nelement(&toparse) > MAXFILE) {

        log_warn("too many file to parse at: ",path) ;
        goto err ;
    }

    FOREACH_SASTR(&toparse,pos) {

        again.len = 0 ;

        char *file = toparse.s + pos ;

        if (!file_readputsa(&again, path, file))
            goto err ;

        if (again.len > MAXENV) {

            log_warn("too many bytes in file: ", path, file) ;
            goto err ;
        }

        if (again.len) {

            _init_stack_(stk, again.len + 1) ;
            size_t npos = 0, mpos = 0 ;
            int kpos = -1, found = 1 ;

            if (!stack_copy(&stk, again.s, again.len) ||
                !stack_close(&stk))
                goto err ;

            again.len = 0 ;
            tokeep.len = 0 ;

            if (!environ_parse_files(&again, stk.s))
                goto err ;

            FOREACH_SASTR(&again, npos) {

                mpos = 0 ;
                found = 1 ;
                kpos = -1 ;
                char *line = again.s + npos ;
                _init_stack_(key, strlen(line) + 1) ;

                if (!stack_copy(&key, line, strlen(line)) ||
                    !stack_close(&key))
                    goto err ;

                if (!environ_clean_key(&key))
                    goto err ;

                if (!pos) {
                    /** First file, nothing to compare.
                     * Copy it verbatim */
                    if (!sastr_add_string(modifs, line))
                        goto err ;

                    continue ;
                }

                /** more than one file, for a same key=value pair
                 * keep the fresh one and or add it if it doesn't exist yet
                 * in the list */
                FOREACH_STK(&smodifs, mpos) {

                    _init_stack_(same, strlen(smodifs.s + mpos) + 1) ;
                    kpos = environ_get_key(&same, smodifs.s + mpos, key.s) ;

                    if (kpos >= 0) {

                        found = 0 ;
                        if (!sastr_add_string(modifs, line))
                            goto err ;

                        break ;
                    }
                }

                /** doesn't exist yet*/
                if (found)
                    if (!sastr_add_string(modifs, line))
                        goto err ;
            }
        }
    }

    if (!stralloc_0(modifs))
        goto err ;

    modifs->len-- ;

    freed:
        e = 1 ;
    err:
        stralloc_free(&toparse) ;
        stralloc_free(&again) ;
        stralloc_free(&tokeep) ;
        return e ;
}