/*
 * scan_timeout.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

int scan_timeout(char const *str, uint32_t *array, int row)
{
    stralloc kp = STRALLOC_ZERO ;
    uint32_t u ;

    if (!sastr_clean_string(&kp,str)) goto err ;

    if (!uint320_scan(kp.s,&u))
    {
        stralloc_free(&kp) ;
        return -1 ;
    }
    if(!*(array+row*UINT_FMT))
    {
        uint32_scan(kp.s,&(*(array+row*UINT_FMT))) ;
    }
    else goto err ;
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}
