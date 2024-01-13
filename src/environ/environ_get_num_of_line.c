/*
 * environ_get_num_of_line.c
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

#include <oblibs/environ.h>
#include <oblibs/stack.h>
#include <oblibs/mill.h>

#include <skalibs/stralloc.h>

unsigned int environ_get_num_of_line(stralloc *sa)
{
    if (!sa->len) return -1 ;
    unsigned int nbline = 0 ;
    _init_stack_(stk, sa->len + 1) ;
    wild_zero_all(&MILL_GET_LINE) ;
    nbline = mill_string(&stk, sa->s, sa->len, &MILL_GET_LINE) ;
    if (nbline == -1) return -1 ;
    nbline = MILL_GET_LINE.inner.nline ;
    return nbline ;
}
