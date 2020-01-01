/* 
 * dir_create_tmp.c
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

#include <oblibs/string.h>

#include <stdlib.h>

#include <skalibs/stralloc.h>

char *dir_create_tmp(stralloc *sa, char const *dst, char const *tname)
{	
	if (dst[0] != '/') return 0 ;
	
	if (!auto_stra(sa,dst,"/",tname,":","XXXXXX")) return 0 ;
	
	return mkdtemp(sa->s) ;
}
