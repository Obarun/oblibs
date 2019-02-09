/* 
 * get_rstrlen_until.c
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

#include <sys/types.h>
#include <string.h>

#include <oblibs/string.h>

ssize_t get_rstrlen_until(char const *str,char const *search)
{
	int i ;
	size_t baselen = strlen(str) ;
	size_t len = baselen ;
	size_t slen = strlen(search) ;
	if (!len || !slen || (len < slen)) return -1 ;
	for (i = slen -1 ; i >= 0 ; i--)
	{
		len--;
		char c = search[i] ;
		if (c != str[len]) return -1 ;
	}

	return baselen - (baselen - len) ;
}
