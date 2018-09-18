/* 
 * dir_scan_absopath.c
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

#include <ctype.h>

#include <oblibs/string.h>

int dir_scan_absopath(char const *s)
{
	int space ;
	space = 0 ;
	while(isspace(s[space])){
		space++;
	}
	return (s[space] != '/') ? -1 : space ;

}
