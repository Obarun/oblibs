/* 
 * oblist_addtail.c
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
 
#include <oblibs/oblist.h>

int oblist_addtail (oblist **list, char const *s, size_t n)
{
	unsigned int r ;
	oblist *tmp ;
	oblist *new ;
	if (!s || !n)
		return 0 ;
	if (!*list)
	{
		r = oblist_new(list,s,n) ;
		if(!r) return 0 ;
		return 1 ;
	}
	r = oblist_new(&new,s,n) ;
	if(!r) return 0 ;
	tmp = *list ;
	while (tmp->n)
		tmp = tmp->n ;
	tmp->n = new ;
	
	return 1 ;
}
