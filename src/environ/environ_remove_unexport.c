/* 
 * environ_remove_unexport.c
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

#include <oblibs/environ.h>

#include <string.h>

#include <oblibs/mill.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int environ_remove_unexport(stralloc *modifs,stralloc *sa)
{
	if (!sa->len) return 0 ;
	size_t pos = 0 ;
	char const search[3] = { '=', VAR_UNEXPORT, 0 } ;
	stralloc cp = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	if (!stralloc_copy(&cp,sa)) goto err ;
	if (!stralloc_0(&cp)) goto err ;
	cp.len-- ;
	if (!sastr_split_string_in_nline(&cp)) goto err ;
	
	for (;pos < cp.len; pos += strlen(cp.s + pos)+1)
	{
		tmp.len = 0 ;
		stralloc_catb(&tmp,cp.s+pos,strlen(cp.s+pos)+1) ;
		
		if (!sastr_replace(&tmp,search,"=")) goto err ;
		if (!stralloc_0(&tmp)) goto err ;
		tmp.len-- ;
		if (!stralloc_catb(modifs,tmp.s,strlen(tmp.s) + 1)) goto err ;
	}
	if (!sastr_rebuild_in_nline(modifs)) goto err ;
	stralloc_free(&cp) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&cp) ;
		stralloc_free(&tmp) ;
		return 0 ;
}
