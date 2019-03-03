/* 
 * obstr_sep.c
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

char *obstr_sep(char **str, const char *delims)
{
	char *token;
	
	if(*str == NULL ) {
		/* No more tokens */
		return *str = NULL ;
	}

	token = *str ;
	while(**str != '\0') {
		if(strchr(delims,**str) != NULL ) {
			**str = '\0' ;
			(*str)++ ;
			return token ;
			
		}
		(*str)++;
	}
	 
	return *str = NULL ;
}
