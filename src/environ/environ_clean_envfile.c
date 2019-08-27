/* 
 * environ_clean_envfile.c
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

#include <skalibs/stralloc.h>

int environ_clean_envfile(stralloc *modifs,stralloc *sa)
{
	if (!environ_get_clean_env(sa)) return 0 ;
	if (!stralloc_0(sa)) return 0 ;
	sa->len-- ;
	if (!environ_clean_nline(sa)) return 0 ;
	if (!environ_remove_unexport(modifs,sa)) return 0 ;
	return 1 ;	
}
