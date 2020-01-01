/* 
 * oblist_new.c
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
 
#include <stdlib.h>
#include <string.h>

#include <oblibs/oblist.h>

int oblist_new(oblist **list, void const *s, size_t len)
{
	oblist *l ;

	if (!(l = malloc(sizeof(oblist))))
		return 0 ;
	if (!s)
	{
		l->s = NULL ;
		l->len = 0 ;
	}
	else
	{
		if (!(l->s = malloc(len+1)))
			return 0 ;
		memcpy(l->s, s, len);
		l->len = len ;
	}
	l->n = NULL ;
	*list = l ;
	return 1 ;
}
