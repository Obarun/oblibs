/* 
 * scan_timeout.c
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
 
#include <sys/types.h>
#include <stdint.h>

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stralist.h>

#include <skalibs/genalloc.h>
#include <skalibs/types.h>

int scan_timeout(char *str, uint32_t *array, int row)
{
	genalloc gatmp = GENALLOC_ZERO ;
	
	if (!clean_val(&gatmp,str)) return 0 ;
	if(!scan_uint32(gastr(&gatmp)))
	{
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return -1 ;
	}
	if(!*(array+row*UINT_FMT)){
		uint32_scan(gastr(&gatmp),&(*(array+row*UINT_FMT))) ;
	}else{
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return 0 ;
	}
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	return 1 ;
}
