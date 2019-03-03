/* 
 * get_nbline_ga.c
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
 
 
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/stralist.h>

#include <skalibs/genalloc.h>

unsigned int get_nbline_ga(char const *str, size_t len, genalloc *ga)
{
	unsigned int pos, loop ;
	ssize_t r ;
	pos = loop = 0 ;

	
	while ((pos < len) && (r != -1))
	{
		r = get_len_until(str+pos,'\n') ;
		if (!stra_new(ga,str+pos,r+1)) return 0 ;
		pos = r+pos+1 ;/**+1 to skip the \n character*/
		loop++ ;
	}
	
	return loop ;
}
