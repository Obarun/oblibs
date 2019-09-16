/* 
 * scan_uidlist.c
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
 
#include <sys/types.h>
#include <stdint.h>

#include <oblibs/types.h>
#include <oblibs/error.h>

int scan_uid(char const *str,uid_t *uid)
{
	int r = 0 ;
	r = get_uidbyname(str,uid) ;
	if(r == -1) return 0 ;
	
	return 1 ;
}
