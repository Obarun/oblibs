/* 
 * stra_free.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>

void stra_free(stralist *stra)
{
	stralloc_free(&stra->sa) ;
	*stra = stralist_zero ;
}
