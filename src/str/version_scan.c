/* 
 * version_scan.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

int version_scan(stralloc *sa, char const *str,uint8_t ndot)
{
	int r ;
	uint8_t dot = 0 ;
	uint32_t num ;
	size_t slen = strlen(str) ;
	char snum[slen + 1] ;
	auto_strings(snum,str) ;
	while(dot < ndot + 1)
	{
		size_t len = strlen(snum) ;
		r = get_rlen_until(snum,'.',len) ;
		if (r == -1 && dot != ndot) return 0 ;
		char tmp[len + 1] ;
		auto_strings(tmp,snum + r + 1) ;
		if (!uint0_scan(tmp,&num)) return 0 ;
		if (!stralloc_inserts(sa,0,tmp)) return 0 ;
		if (dot < ndot) 
			if (!stralloc_inserts(sa,0,".")) return 0 ;
		dot++ ;
		snum[r] = 0 ;
	}
	return 1 ;
}
