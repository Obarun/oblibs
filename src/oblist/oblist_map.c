/* 
 * oblist_map.c
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
 
#include <stddef.h>
#include <stdlib.h>

#include <oblibs/oblist.h>


oblist *oblist_map(oblist *list, oblist *(*f)(oblist *data))
{
	oblist *nx ;
	oblist *l ;

	if (!list)
		return NULL ;
	l = f(list) ;
	nx = list ;
	while (list->n)
	{
		list = list->n ;
		if (!(l->n = f(list)))
		{
			free(l->n) ;
			return NULL ;
		}
		l = l->n ;
	}
	return nx ; 
}
