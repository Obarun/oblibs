/*
 * scan_uidlist_wdelim.c
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

#include <sys/types.h>

#include <oblibs/types.h>
#include <oblibs/mill.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int scan_uidlist_wdelim(char const *str, uid_t *array,int delim)
{
    int r ;
    stralloc kp = STRALLOC_ZERO ;
    stralloc sa = STRALLOC_ZERO ;
    parse_mill_t MILL_GET_DELIM = {
        .close = delim, .forceopen = 1, .keepopen = 1,
        .forceclose = 1 , .skip = " \t\r", .skiplen = 3,
        .forceskip = 1, .inner.debug = "get_delim" } ;

    if (!stralloc_cats(&sa,str) ||
    !stralloc_0(&sa)) goto err ;

    wild_zero_all(&MILL_GET_DELIM) ;

    r = mill_string(&kp,&sa,&MILL_GET_DELIM) ;
    if (r == -1 || !r) goto err ;
    if (!sastr_rebuild_in_oneline(&kp)) goto err ;
    if (!scan_uidlist(kp.s,array)) goto err ;

    stralloc_free(&sa) ;
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&sa) ;
        stralloc_free(&kp) ;
        return 0 ;
}
