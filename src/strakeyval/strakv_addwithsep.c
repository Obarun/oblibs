/* 
 * strakv_addwithsep.c
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
 
#include <oblibs/strakeyval.h>

#include <stddef.h>

#include <oblibs/error2.h>
#include <oblibs/string.h>

#include <skalibs/genalloc.h>

int strakv_addwithsep(genalloc *ga,char *s,char const *sep)
{
	char *k = NULL ;
	char *v = NULL ;
		
	strakeyval tmp = STRAKEYVAL_ZERO ;
	
	k = s ;
	v = s ;
	obstr_sep(&v,sep) ;
	if (v == NULL) goto err ;
	if (!obstr_trim(v,' ')) goto err ;
	obstr_trim(v,'\n') ;
	if (!obstr_trim(k,' ')) goto err ;
	
	if(!stralloc_catb(&tmp.key,k,strlen(k))) goto err ; 
	if(!stralloc_0(&tmp.key)) goto err ;
	if(!stralloc_catb(&tmp.val,v,strlen(v))) goto err ;
	if(!stralloc_0(&tmp.val)) goto err ;
	tmp.n++;
	if(!genalloc_append(strakeyval,ga,&tmp)) goto err ;
		
	return 1 ;
	err:
		strakv_free(&tmp) ;
		return 0 ;
}
