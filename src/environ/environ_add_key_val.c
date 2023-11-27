/*
 * environ_add_key_val.c
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

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <execline/execline.h>

int environ_add_key_val (const char *key, const char *val, exlsn_t *info)
{
    int r ;
    eltransforminfo_t si = ELTRANSFORMINFO_ZERO ;
    elsubst_t blah ;

    blah.var = info->vars.len ;
    blah.value = info->values.len ;

    if (el_vardupl (key, info->vars.s, info->vars.len)) goto err ;
    if (!stralloc_catb (&info->vars, key, strlen(key) + 1)) goto err ;
    if (!stralloc_catb (&info->values, val, strlen(val))) goto err ;

    r = el_transform (&info->values, blah.value, &si) ;
    if (r < 0) goto err;
    blah.n = r ;

    if (!genalloc_append (elsubst_t, &info->data, &blah)) goto err ;

    return 1 ;
    err:
        info->vars.len = blah.var ;
        info->values.len = blah.value ;
        return 0 ;
}
