/* 
 * strakv_free.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#include <oblibs/strakeyval.h>

#include <skalibs/alloc.h>

void strakv_free(strakeyval *stra)
{
	alloc_free(stra->key.s) ;
	alloc_free(stra->val.s) ;
	*stra = strakeyval_zero ;
}
