/*
 * dir_rm_rf.c
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

#include <oblibs/directory.h>

#include <skalibs/skamisc.h>
#include <skalibs/djbunix.h>

int dir_rm_rf (char const *dir)
{
   int r = rm_rf(dir) ;
   stralloc_free(&satmp) ;
   return r < 0 ? 0 : 1 ;
}
