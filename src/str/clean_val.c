/* 
 * clean_val.c
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <oblibs/string.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/bytes.h>
#include <oblibs/memory.h>


int clean_val(genalloc *ga,char const *line)
{
	size_t len = strlen(line) ; 
	int i,pos,j ;
	i = pos = j = 0 ;
	size_t inlen = len + 1 ;/** the end of string may not contain space*/
	char key[len + 1] ; 
	for (;(size_t) i<inlen;i++)
	{
		if(isspace(line[i]) || !line[i])/** !line[i] end of string without space*/
		{
			if(pos){
				key[j] = 0 ;
				if(!stra_add(ga,key)) return 0  ;
				byte_tozero(key,j) ;
				j = 0 ;
			}
			pos = 0;
			continue;
		}
		key[j++] = line[i] ;
		pos = 1 ;
	}
		
	return 1 ;
}
