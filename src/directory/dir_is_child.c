/*
 * dir_is_child.c
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

#include <string.h>

#include <oblibs/string.h>

int dir_is_child(char const *parent, char const *child)
{
    if ((parent[0] != '/') || (child[0] != '/'))
        return 0 ;

    int r = str_start_with(child,parent) ;

    if (!r) return 0 ;

    size_t len = strlen(parent) ;

    if (parent[len - 1] ==  '/') len-- ;

    if ((child[len] != '/'))
        return 0 ;

    return 1 ;

}
