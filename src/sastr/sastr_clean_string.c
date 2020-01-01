/* 
 * sastr_clean_string.c
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

#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int sastr_clean_string(stralloc *sa,char const *string)
{
	if (!stralloc_cats(sa,string) ||
	!stralloc_0(sa)) return 0 ;
	if (!sastr_clean_element(sa)) return 0 ;
	return 1 ;
}
