/* 
 * sastr_replace.c
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
#include <oblibs/string.h>

#include <string.h>

#include <skalibs/stralloc.h>

int sastr_replace (stralloc *sa,char const *regex,char const *by)
{
	if (!sa->len || !*regex || !strcmp(regex,by)) return 0 ;
	size_t i = 0, j = 0, slen = strlen(regex), blen = strlen(by) ;
	unsigned int found = 0 ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc shrink = STRALLOC_ZERO ;

	for (; i < sa->len; i++)
	{
		found = 0 ; j = 0 ;
		for (;j < slen;j++)
		{
			char c = regex[j] ;
			if (c != sa->s[i+j]){ found = 0 ; break ; }
			found++ ;
		}
		if (found){
			if (!auto_stra(&tmp,by,sa->s + i + slen)) goto err ;
			shrink.len = 0 ;
			if (!auto_stra(&shrink,tmp.s + i + blen)) goto err ;
			if (!sastr_replace(&shrink,regex,by)) goto err ;
			tmp.len = i + blen ;
			if (!auto_stra(&tmp,shrink.s)) goto err ;
			break ;			
		}
		if (!stralloc_catb(&tmp,sa->s+i,1)) goto err ;	
	}
	if (!stralloc_copy(sa,&tmp)) goto err;
	stralloc_free(&tmp) ;
	stralloc_free(&shrink) ;
	return 1 ;
	err:
		stralloc_free(&tmp) ;
		stralloc_free(&shrink) ;
		return 0 ;
}
