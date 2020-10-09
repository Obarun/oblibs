/* 
 * sastr_drop_same_element.c
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

#include <string.h>

#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int sastr_drop_same_element(stralloc *sa)
{
	stralloc tmp = STRALLOC_ZERO ;

	size_t pos = 0 ;

	char *p1 = 0, *p2 = 0 ;

	for (; pos < sa->len ; pos += strlen(sa->s + pos) + 1)
	{
		p1 = sa->s + pos ;
		p2 = sa->s + pos + strlen(p1) + 1 ;
		if (!strcmp(p1,p2)) continue ;
		if (!sastr_add_string(&tmp,p1)) return 0 ;
	}

	if (!stralloc_copy(sa,&tmp)) return 0 ;
	stralloc_free(&tmp) ;

	return 1 ;
}

