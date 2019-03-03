/* 
 * stra_cpytobuf.c
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

#include <string.h>
 
#include <oblibs/stralist.h>

#include <skalibs/genalloc.h>

void stra_cpytobuf(char *buf,genalloc *ga)
{
	size_t i = 0;
	for (;i<genalloc_len(stralist,ga);i++)
		strcat(buf,gaistr(ga,i)) ;
	return ;
}
