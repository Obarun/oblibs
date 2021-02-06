/*
 * environ_get_commented_line.c
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
#include <stdint.h>
#include <unistd.h>

#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int environ_drop_previous_same_key(stralloc *list,stralloc *newlist)
{
    size_t pos = 0, gpos = 0 ;
    ssize_t lequal = -1, kequal = -1;
    uint8_t found = 1 ;

    if (!sastr_split_string_in_nline(list))
        return 0 ;

    if (!sastr_split_string_in_nline(newlist))
        return 0 ;

    stralloc tmp = STRALLOC_ZERO ;

    FOREACH_SASTR(list,pos) {

        found = 1 ;
        lequal = get_len_until(list->s + pos,'=') ;

        if (lequal == -1)
            goto err ;

        char lkey[lequal + 1] ;
        memcpy(lkey,list->s + pos,lequal) ;
        lkey[lequal] = 0 ;

        {
            gpos = 0 ;
            FOREACH_SASTR(newlist,gpos) {

                kequal = get_len_until(newlist->s + gpos,'=') ;
                if (kequal == -1)
                    goto err ;

                char key[kequal + 1] ;
                memcpy(key,newlist->s + gpos ,kequal) ;
                key[kequal] = 0 ;

                if (!strcmp(key,lkey))
                    found = 0 ;

                 if (sastr_find(&tmp,newlist->s + gpos) == -1)
                    if (!sastr_add_string(&tmp,newlist->s + gpos))
                        goto err ;
            }

        }

        if (found && (sastr_find(&tmp,list->s + pos) == -1))
            if (!sastr_add_string(&tmp,list->s + pos))
                goto err ;
    }

    if (!stralloc_copy(list,&tmp))
        goto err ;

    stralloc_free(&tmp) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;

}


