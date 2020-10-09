/* 
 * sastr_sortndrop_element.c
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

int sastr_sortndrop_element(stralloc *sa)
{
	if (!sastr_sort(sa)) return 0 ;
	return sastr_drop_same_element(sa) ;
}


