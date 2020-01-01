/* 
 * obstr_replace.c
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

void obstr_replace(char *str,int a,int b)
{
	
	unsigned int i ;
	size_t len = strlen(str) ;
	char result[len+1] ;
	
	for(i = 0 ;i < len; i++)
	{
		if (str[i] == a)
		{
			result[i] = b ;
		}else
			result[i] = str[i] ;
	}
	result[len] = 0 ;
	memcpy(str,result,len) ;
	str[len] = 0 ;
}
