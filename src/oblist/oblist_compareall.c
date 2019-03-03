/* 
 * oblist_compareall.c
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

int oblist_compareall(oblist *list,char const *s)
{
	oblist *tmp ;
	tmp = list ;
	while (tmp){
		if (oblist_compare(tmp,s)) return 1 ;
		tmp = tmp->n ;
	}
	return 0 ;
}
