/* 
 * str_append_check.c
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

#include <sys/types.h>

#include <oblibs/string.h>

void str_append_check (char *str,char c)
{
	size_t len = 0 ;
	char *base = str ;
	for (;*base;base++) len++ ;
		
	if (str[len-1] != c ){
		str[len] = c ;
		len++ ;
		str[len] = 0 ;
	}
}
