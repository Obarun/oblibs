/* 
 * clean_val_wdelim.c
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

#include <oblibs/string.h>

#include <skalibs/genalloc.h>

int clean_val_wdelim(genalloc *ga,char const *line,int delim)
{
	size_t slen = strlen(line) ;
	char s[slen] ;
	memcpy(s,line,slen) ;
	s[slen] = 0 ;
	obstr_replace(s,delim,' ') ;

	if (!clean_val(ga,s)) return 0 ;
		
	return 1 ;
}
