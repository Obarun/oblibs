/* 
 * sastr_clean_string_wdelim.c
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

#include <oblibs/sastr.h>
#include <oblibs/mill.h>

#include <string.h>

#include <skalibs/stralloc.h>

int sastr_clean_string_wdelim(stralloc *sa,char const *str,unsigned int delim)
{
	
	int r ;
	stralloc kp = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	size_t pos = 0 ;
		
	parse_mill_t mill_delim = {
		.close = delim, .forceopen = 1, .keepopen = 1,
		.forceclose = 1, .inner.debug = "delim" } ;

	wild_zero_all(&mill_delim) ;

	if (!stralloc_cats(&kp,str) ||
	!stralloc_0(&kp)) goto err ;

	r = mill_string(&tmp,&kp,&mill_delim) ;
	if (r == -1 || !r) goto err ;
	
	for (size_t i = 0 ; i < tmp.len; i += strlen(tmp.s + i) +1)
	{
		kp.len = 0 ;
		if (!stralloc_catb(&kp, tmp.s + i,strlen(tmp.s + i)) ||
		!stralloc_0(&kp)) goto err ;
		if (!sastr_clean_element(&kp)) goto err ;
		if (!sastr_rebuild_in_oneline(&kp)) goto err ;
		if (!sastr_add_string(sa,kp.s)) goto err ;
	}
	stralloc_free(&kp) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		return 0 ;
}

