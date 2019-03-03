/* 
 * obstr_equal.c
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

#include <string.h>

#include <oblibs/string.h>

int obstr_equal(const char *str1, const char *str2)
{
	int r ;
	r = strcmp(str1, str2) ;
	if (!r) return 1 ;
	return 0 ;
}
