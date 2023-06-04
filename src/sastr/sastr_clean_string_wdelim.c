/*
 * sastr_clean_string_wdelim.c
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

#include <string.h>

#include <oblibs/sastr.h>
#include <oblibs/mill.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int sastr_clean_string_wdelim(stralloc *sa,char const *str,unsigned int delim)
{

    int r ;
    stralloc kp = STRALLOC_ZERO ;
    size_t pos = 0, len = strlen(str) ;
    _init_stack_(stk, len + 1) ;

    parse_mill_t mill_delim = {
        .close = delim, .forceopen = 1, .keepopen = 1,
        .forceclose = 1, .inner.debug = "delim" } ;

    wild_zero_all(&mill_delim) ;

    if (!auto_stra(&kp,str)) goto err ;

    r = mill_string(&stk, str, len, &mill_delim) ;
    if (r == -1 || !r) goto err ;
    if (!stack_close(&stk)) goto err ;

    FOREACH_STK(&stk, pos) {

        kp.len = 0 ;
        if (!stralloc_catb(&kp, stk.s + pos, strlen(stk.s + pos)) ||
        !stralloc_0(&kp)) goto err ;
        if (!sastr_clean_element(&kp)) goto err ;
        if (!sastr_rebuild_in_oneline(&kp)) goto err ;
        if (!sastr_add_string(sa,kp.s)) goto err ;
    }
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}

