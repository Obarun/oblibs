/* 
 * stralloc_obreplace.c
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

#include <oblibs/stralist.h>

#include <oblibs/error2.h>

#include <skalibs/stralloc.h>
#include <skalibs/posixplz.h>

int stralloc_obreplace(stralloc *sa, char const *s)
{
	size_t slen = 0 ;
	slen = strlen(s) ;
	sa->len = 0 ;
	if (!stralloc_catb(sa,s,slen)) return 0 ;
	if (!stralloc_0(sa)) return 0 ;
	return 1 ;
}
