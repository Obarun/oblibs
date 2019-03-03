/* 
 * stralist_new.c
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
 
#include <stddef.h>

#include <oblibs/stralist.h>
#include <oblibs/error.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

int stra_new(genalloc *ga,char const *s,size_t len)
{
	stralloc tmp = STRALLOC_ZERO ;
	if (!stralloc_catb(&tmp,s,len)) return 0 ;
	if (!stralloc_0(&tmp)) return 0 ;
	if (!genalloc_append(stralist,ga,&tmp)) return 0 ;
	return 1 ;
}
