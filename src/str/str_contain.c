/* 
 * str_contain.c
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

int str_contain(char const *str, char const *search)
{
	if (!*str || !*search) return 0 ;
	
	size_t i = 0, j = 0, len = strlen(str), slen = strlen(search) ;
	int found = 0 ;
	
	for (; i < len ; i++){
		
		found = 0 ; j = 0 ;
		
		for (;j < slen ; j++) {
			
			char c = search[j] ;
			
			if (c != str[i+j]) { found = 0 ; break ; }
			
			found++ ;
		}
		
		if (found) break ;
	}
	
	return found ? 1 : 0 ;
}
