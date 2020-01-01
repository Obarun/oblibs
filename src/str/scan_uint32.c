/* 
 * scan_uint32.c
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
 
#include <sys/types.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/types.h>

int scan_uint32(char const *s)
{
	char buff[UINT_FMT] ;
	uint32_t u ;
	ssize_t r ;
	size_t slen = strlen(s) ;
	memcpy(buff,s,slen) ;
	buff[slen] = 0 ;
	r = get_len_until(buff,'\n') ;
	buff[r] = 0 ;
	return (!uint320_scan(buff,&u)) ? 0 : 1 ;
}
