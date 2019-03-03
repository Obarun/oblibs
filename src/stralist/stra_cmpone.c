/* 
 * stra_cmpone.c
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
 
#include <oblibs/stralist.h>
#include <oblibs/string.h>

#include <skalibs/genalloc.h>

int stra_cmpone(genalloc *ga,char const *s,unsigned int idx)
{
	size_t len = genalloc_len(stralist,ga) ;
	if ( (size_t) idx > len) return 0 ;
	char const *scmp ;
	scmp = (char *)gaistr(ga,idx) ;
	return obstr_equal(s,scmp) ?  1 : 0 ;
}
