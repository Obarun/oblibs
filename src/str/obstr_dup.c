/* 
 * obstr_dup.c
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

#include <stdlib.h>
#include <string.h>

#include <oblibs/string.h>

char *obstr_dup(const char *str)
{
	int i ;
	char *ptr = 0 ;
	if (!(ptr=malloc(sizeof(char) * strlen(str)+1)))
		return 0 ;
	i = 0 ;
	while(str[i])
	{
		ptr[i]=str[i] ;
		i++ ;
	}
	ptr[i] = 0 ;
	
	return ptr ;
}
