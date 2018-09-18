/* 
 * stra_remove.c
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

int stra_remove(genalloc *ga, char const *s)
{
	int idx ;
	idx = stra_findidx(ga,s) ;
	if (idx == -1) return 0 ;
	
	return stra_removeidx(ga,idx) ? 1 : 0 ;
}
