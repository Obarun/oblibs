/* 
 * sastr_replace_all.c
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

#include <oblibs/sastr.h>

#include <string.h>

#include <skalibs/stralloc.h>

int sastr_replace_all(stralloc *sa,char const *regex,char const *by)
{
	size_t i = 0 ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc result = STRALLOC_ZERO ;
		
	if (!sastr_split_string_in_nline(sa)) goto err ;
	for (;i < sa->len; i += strlen(sa->s + i) + 1)
	{
		tmp.len = 0 ;
		if (!stralloc_catb(&tmp,sa->s + i,strlen(sa->s+i))) goto err ;
		if (!stralloc_0(&tmp)) goto err ;
		if (!sastr_replace(&tmp,regex,by)) goto err ;
		if (!stralloc_catb(&result,tmp.s,strlen(tmp.s) + 1)) goto err ;
	}
	if (!sastr_rebuild_in_nline(&result) ||
	!stralloc_copy(sa,&result)) goto err ;
	
	stralloc_free(&result) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&result) ;
		stralloc_free(&tmp) ;
		return 0 ;
}
