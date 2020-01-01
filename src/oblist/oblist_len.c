/* 
 * oblist_len.c
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

#include <stddef.h>
#include <oblibs/oblist.h>

size_t oblist_len(oblist *list)
{
	size_t i;
	oblist *tmp ;
	if (!list)
		return 0 ;
	i = 1 ;
	tmp = list ;
	while (tmp->n)
	{
		tmp = tmp->n ;
		i++ ;
	}
	return i ;
}
