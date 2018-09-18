/* 
 * dir_create_tmp.c
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

#include <oblibs/directory.h>

#include <stdlib.h>
#include <errno.h>

#include <skalibs/stralloc.h>

char *dir_create_tmp(stralloc *tmp,char const *dst, char const *tname)
{	
	int fdscan ;
	
	fdscan = dir_scan_absopath(dst) ;
	if (fdscan < 0) return 0 ;
	
	if (!stralloc_cats(tmp,dst)) return 0 ;
	if (!stralloc_cats(tmp,"/")) return 0 ;
	if (!stralloc_cats(tmp,tname)) return 0 ;
	if (!stralloc_cats(tmp,":")) return 0 ;
	if (!stralloc_cats(tmp,"XXXXXX")) return 0 ;
	if (!stralloc_0(tmp)) return 0 ;
	
	return mkdtemp(tmp->s) ;
}
