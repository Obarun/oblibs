/*
 * environ_substitute.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/env.h>//env_get2

#include <execline/execline.h>

int environ_substitute(char const *key, char const *val,exlsn_t *info, char const *const *envp,int unexport)
{
    char const *defaultval = "" ;
    char const *x ;

    eltransforminfo_t si = ELTRANSFORMINFO_ZERO ;
    elsubst_t blah ;

    blah.var = info->vars.len ;
    blah.value = info->values.len ;

    if (el_vardupl(key, info->vars.s, info->vars.len)) goto err ;
    if (!stralloc_catb(&info->vars,key, strlen(key) + 1)) goto err ;

    x = env_get2(envp, key) ;
    if (!x) x = defaultval ;
    else if (unexport)
    {
        if (!stralloc_catb(&info->modifs, key, strlen(key) + 1)) goto err ;
    }
    if (!x) blah.n = 0 ;
    else
    {
        int r ;
        if (!stralloc_cats(&info->values, x)) goto err ;
        r = el_transform(&info->values, blah.value, &si) ;
        if (r < 0) goto err ;
        blah.n = r ;
    }
    if (!genalloc_append(elsubst_t, &info->data, &blah)) goto err ;

    return 1 ;

    err:
        info->vars.len = blah.var ;
        info->values.len = blah.value ;
        return 0 ;
}
