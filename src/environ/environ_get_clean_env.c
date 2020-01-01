/* 
 * environ_get_clean_env.c
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

#include <oblibs/environ.h>

#include <string.h>
#include <oblibs/mill.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

int environ_get_clean_env(stralloc *sa)
{
	if (!sa->len) return 0 ;
	int r ;
	size_t pos = 0, newpos = 0 ;
	uint32_t nbline ;
	stralloc src = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	wild_zero_all(&MILL_GET_LINE) ;
	nbline = mill_string(&src,sa,&MILL_GET_LINE) ;
	if (nbline == -1) goto err ;
	if (!sastr_rebuild_in_nline(&src)) goto err ;
	wild_zero_all(&MILL_GET_LINE) ;
	wild_zero_all(&MILL_GET_COMMENT) ;
	while (pos < src.len)
	{
		newpos = pos ;
		r = mill_element(&tmp,src.s,&MILL_GET_COMMENT,&pos) ;
		if (r == -1) goto err ;
		if (!r) {
			r = mill_element(&tmp,src.s,&MILL_GET_LINE,&newpos) ;
			if (r == -1 || !r) goto err ;
			pos = MILL_GET_LINE.inner.pos ;
		}else pos = MILL_GET_COMMENT.inner.pos ;
		nbline-- ;
	}
	if (!sastr_rebuild_in_nline(&tmp)) goto err ;
	if (!stralloc_copy(sa,&tmp)) goto err ;
	stralloc_free(&src) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&src) ;
		stralloc_free(&tmp) ;
		return 0 ;
}
