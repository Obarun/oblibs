/* 
 * environ_rebuild_line.c
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

#include <string.h>
#include <sys/types.h>//ssize_t

#include <oblibs/string.h>//get_len_until
#include <oblibs/mill.h>

#include <skalibs/stralloc.h>

int environ_rebuild_line(stralloc *sa)
{
	if (!sa->len) return 0 ;
	ssize_t r ;
	size_t pos = 0, len ;
	unsigned int found = 0, idx = 0, space = 0 ;
	stralloc tmp = STRALLOC_ZERO ;
	for (;pos < sa->len; pos += strlen(sa->s + pos)+1)
	{
		len = strlen(sa->s+pos) ;
		if (!stralloc_catb(&tmp,sa->s+pos,strlen(sa->s+pos))) goto err ;
		r = get_len_until(sa->s+pos,'=') ;
		if (r >= 0) found++ ;
		/**			  case key= 		case '=' 			multiple '=' ? */
		if ((!found || (r == len-1) || (!r && (len == 1))) && found < 2) continue ;
		/* case '=val'			case key=val */	
		if ((!r && len > 1) || (r > 1 && !idx)) {
			if (!stralloc_catb(&tmp," ",1)) goto err ;
			space++ ;
		}
		else { 
			if (!stralloc_catb(&tmp," ",1)) goto err ;
			space++ ;
		}
		idx++ ;
	}
	// remove the last ' '
	if (space) tmp.len--;
	if (!stralloc_copy(sa,&tmp)) goto err ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}
