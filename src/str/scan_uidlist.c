/*
 * scan_uidlist.c
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

#include <sys/types.h>
#include <string.h>

#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int scan_uidlist(char const *str, uid_t *array)
{
    int r ;
    size_t pos = 0 ;
    uid_t uid, nuser ;

    stralloc tmp = STRALLOC_ZERO ;

    r = uid = 0 ;
    nuser = array[0] ;

    if (!sastr_clean_string(&tmp,str)) return 0 ;

    for (;pos < tmp.len; pos += strlen(tmp.s + pos)+1)
    {
        char *val = tmp.s + pos ;
        r = get_uidbyname(val,&uid) ;
        if(r == -1) goto err ;
        nuser++;
        array[0] = nuser ;
        array[nuser] = uid ;
    }
    stralloc_free(&tmp) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
