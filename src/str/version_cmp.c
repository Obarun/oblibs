/* 
 * version_cmp.c
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

#include <skalibs/types.h>

int version_cmp(char const  *a, char const *b, uint8_t ndot)
{
	int ar = 0 , br = 0, ap = 0 , bp = 0 ;
	uint8_t dot = 0 ;
	uint32_t anum, bnum ;
	size_t alen = strlen(a) ;
	size_t blen = strlen(b) ;

	while (dot < ndot + 1)
	{
		char one[alen + 1], two[blen + 1] ;
		auto_strings(one,a+ar) ;
		auto_strings(two,b+br) ;
		ap = get_len_until(a+ar,'.') ;
		bp = get_len_until(b+br,'.') ;
		one[ap] = 0 ;
		two[bp] = 0 ;
		ar += ++ap, br += ++bp ;
		if (!uint0_scan(one,&anum)) return -2 ;
		if (!uint0_scan(two,&bnum)) return -2 ;
		if (anum > bnum) return 1;
		if (anum < bnum) return -1 ;
		dot++ ;
	}
	return 0 ;
}
