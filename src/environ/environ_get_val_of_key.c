/* 
 * environ_get_val_of_key.c
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
#include <sys/types.h>

#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int environ_get_val_of_key(stralloc *sa,char const *key)
{
	if (!sa->len) return 0 ;
	ssize_t pos = -1 ;
	pos = sastr_find(sa,key) ;
	if (pos == -1) return 0 ;
	if (!environ_get_key_nclean(sa,(size_t *)&pos)) return 0 ;
	pos-- ;
	if (!environ_get_val(sa,(size_t *)&pos)) return 0 ;
	return 1 ;
}
