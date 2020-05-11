/* 
 * sastr_find_element_byname.c
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

#include <oblibs/sastr.h>

#include <string.h>
#include <sys/types.h>

#include <skalibs/stralloc.h>

ssize_t sastr_find_element_byname(stralloc *sa,char const *str,unsigned int *el)
{
	ssize_t pos = 0, idx = 0 ;

	for(; (size_t)pos < sa->len; pos += strlen(sa->s + pos) + 1, idx++)
	{
		if (!strcmp(sa->s + pos,str))
		{
			(*el) = idx ;
			return pos ;
		}
	}
	return -1 ;	
}

