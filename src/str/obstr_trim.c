/* 
 * obstr_trim.c
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

#include <string.h>

#include <oblibs/string.h>

int obstr_trim(char *str, int occ) {
	
	size_t slen = strlen(str) ;
	char result[slen + 1 + 1] ;
	char *psearch = 0 ;
	char *presult = 0 ;
	unsigned int index = 0 ;
	psearch = str ;
	
	presult = result ;
	
	if (!str || *str == '\0') {
		return 0 ;
	}
	
	memset(result, '\0',slen + 1) ;
	
	for(;index<strlen(psearch);index++){
		if (!memchr(&psearch[index], occ,1)){
			
			memcpy(presult, &psearch[index],1);
			presult++ ;
		
		}
	}
	if ( str != result){
		memcpy(str, result, (strlen(result)+1)) ;
	}
	
	return 1 ;
}
