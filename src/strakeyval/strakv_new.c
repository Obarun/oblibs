/* 
 * strakv_new.c
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
 
#include <oblibs/strakeyval.h>

#include <stddef.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

int strakv_new(genalloc *ga,char const *key,size_t klen,char const *val,size_t vlen)
{
	strakeyval tmp = STRAKEYVAL_ZERO ;
	if(!stralloc_catb(&tmp.key,key,klen)) return 0 ; 
	if(!stralloc_0(&tmp.key)) return 0 ;
	if(!stralloc_catb(&tmp.val,val,vlen)) return 0 ;
	if(!stralloc_0(&tmp.val)) return 0 ;
	tmp.n = genalloc_len(strakeyval,ga) ;
	if(!genalloc_append(strakeyval,ga,&tmp)) return 0 ;
	
	tmp = strakeyval_zero ;
	strakv_free(&tmp) ;
	
	return 1 ;
}
