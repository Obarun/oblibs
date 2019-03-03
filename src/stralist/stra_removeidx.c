/* 
 * stra_removeidx.c
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

#include <stddef.h>

#include <oblibs/stralist.h>
#include <oblibs/bytes.h>

#include <skalibs/genalloc.h>

int stra_removeidx(genalloc *ga, unsigned int idx)
{
	size_t len = genalloc_len(stralist,ga) ;
	if (idx >= len) return 0 ;
	
	for(;idx<len;idx++){
		if(idx == len-1){ /**end of the array */
			stra_free(&genalloc_s(stralist,ga)[idx]) ;
			break ;
		}
		stra_free(&genalloc_s(stralist,ga)[idx]) ;
		stralloc_copy(&genalloc_s(stralist,ga)[idx].sa,&genalloc_s(stralist,ga)[idx+1].sa) ;
	}

	genalloc_setlen(stralist,ga,len-1) ;

	return 1 ;
}
