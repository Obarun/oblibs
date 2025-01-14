/*
 * environ_create_environ.c
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
#include <stddef.h>

#include <oblibs/sastr.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>
#include <skalibs/env.h>

uint8_t environ_create_environ(char const **nenvp, char const *const *env, stralloc *modifs)
{
    size_t elen = environ_length(env) ;
    size_t n = elen + 1 + sastr_nelement(modifs) ;

    if (!env_merge(nenvp, n , env, elen, modifs->s, modifs->len))
        return 0 ;

    return 1 ;
}
