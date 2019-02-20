/* 
 * scan_uidlist_wdelim.c
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

#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/stralist.h>


#include <skalibs/genalloc.h>

int scan_uidlist_wdelim(char const *str, uid_t *array,int delim) 
{
	int r ;
	size_t nuser ;
	uid_t uid ;

	genalloc gatmp = GENALLOC_ZERO ;
	
	r = uid = nuser = 0 ;
	if (!clean_val_wdelim(&gatmp,str,delim))
	{
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return 0 ;
	}
	for (unsigned int i = 0;i<genalloc_len(stralist,&gatmp);i++)
	{
		r = get_uidbyname(gaistr(&gatmp,i),&uid) ;
		if(!r){
			genalloc_deepfree(stralist,&gatmp,stra_free) ;
			return 0 ;
		}
		nuser++ ;
		array[0] = nuser ;
		array[i+1] = uid ;
	}
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
			
	return 1 ;
}
