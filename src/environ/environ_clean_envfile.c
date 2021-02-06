/*
 * environ_clean_envfile.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

int environ_clean_envfile(stralloc *modifs,char const *path)
{
    int r ;
    size_t pos = 0 ;

    stralloc toparse = STRALLOC_ZERO ;
    stralloc again = STRALLOC_ZERO ;
    stralloc commented = STRALLOC_ZERO ;

    r = sastr_dir_get(&toparse,path,"",S_IFREG) ;

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

        if (!file_readputsa(&again,path,file))
            goto err ;

        if (again.len) {

            if (!sastr_split_string_in_nline(&again))
                goto err ;

            /** file can be empty or just with " \n\t\r" */
            if (again.len) {

                if (!environ_clean_nline(&again) ||
                    !stralloc_0(&again))
                        goto err ;

                if (pos)
                    if (!environ_get_commented_line(&commented,&again))
                        goto err ;

                if (!environ_drop_commented_line(&again))
                    goto err ;

                {
                    /** may be badly formated */
                    size_t tpos = 0 ;
                    int nline = 0 ;
                    FOREACH_SASTR(&again,tpos) {
                        nline++ ;
                        if (!environ_keyvalue_isvalid(again.s + tpos))
                            goto err ;
                    }

                    if (nline > MAXVAR) {

                        log_warn("too many variables in file: ", path, file) ;
                        goto err ;
                    }
                }

                if (!pos) {

                    if (!sastr_add_string(modifs,again.s))
                        goto err ;

                } else {

                    if (pos)
                        if (!environ_drop_previous_same_key(modifs,&again))
                            goto err ;
                }

                if (again.len > MAXENV) {

                    log_warn("too many bytes in file: ", path, file) ;
                    goto err ;
                }
            }
        }
    }

    again.len = 0 ;

    if (commented.len) {

        size_t cpos = 0, mpos = 0, len = modifs->len ;
        ssize_t cequal = -1, mequal = -1 ;
        char tmp[len + 1] ;
        memset(tmp,0,len) ;

        if (!sastr_split_string_in_nline(&commented))
            goto err ;

        FOREACH_SASTR(&commented,cpos) {

            cequal = get_len_until(commented.s + cpos,'=') ;

            /** Real comment should not contain '=' */
            if (cequal == -1)
                continue ;

            cequal-- ;
            char ckey[cequal + 1] ;
            memcpy(ckey,commented.s + cpos + 1 ,cequal) ;
            ckey[cequal] = 0 ;

            {
                mpos = 0 ;
                FOREACH_SASTR(modifs,mpos) {

                    mequal = get_len_until(modifs->s + mpos,'=') ;

                    if (mequal == -1)
                        goto err ;

                    char mkey[mequal + 1] ;
                    memcpy(mkey,modifs->s + mpos ,mequal) ;
                    mkey[mequal] = 0 ;

                    if (strcmp(mkey,ckey))
                        continue ;

                    if (!sastr_add_string(&again,modifs->s + mpos))
                        goto err ;
                }
            }
        }

        {
            pos = 0 ;
            FOREACH_SASTR(&again,pos)
                if (!sastr_remove_element(modifs,again.s + pos))
                    goto err ;
        }

    }

    if (!stralloc_0(modifs))
        goto err ;

    modifs->len-- ;

    freed:
    stralloc_free(&toparse) ;
    stralloc_free(&again) ;
    stralloc_free(&commented) ;
    return 1 ;
    err:
        stralloc_free(&toparse) ;
        stralloc_free(&again) ;
        stralloc_free(&commented) ;
        return 0 ;
}
