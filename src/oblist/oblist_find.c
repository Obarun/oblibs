/* 
 * oblist_find.c
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
#include <oblibs/string.h>

oblist *oblist_find(oblist **list, char const *s)
{
	oblist *tmp ;
	tmp = *list ;
	while(tmp){
		if (oblist_compare(tmp,s)) return tmp ;
		tmp = tmp->n ;
	}
	return NULL ;
}
