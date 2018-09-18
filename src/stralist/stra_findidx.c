/* 
 * stra_findidx.c
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
 
#include <oblibs/stralist.h>

#include <skalibs/genalloc.h>

int stra_findidx(genalloc *ga,char const *s)
{
	for (unsigned int i = 0 ; i < genalloc_len(stralist,ga) ; i++)
		if (stra_cmpone(ga,s,i)) return i ;

	return -1 ;
}
