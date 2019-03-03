/* 
 * dir_create_under.c
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

#include <oblibs/directory.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/types.h>

ssize_t dir_create_under(char const *dst, char const *name, int mode)
{
	int r = 0;
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(name) ;
	size_t slash = 0 ;
	char dir[dstlen + namelen + 2 + 1] ;
	char parentdir[dstlen + 1] ;
	
	r = dir_scan_absopath(dst) ;
	if(r < 0) return 0 ;
	strcpy(dir,dst) ;
	dir_unslash(dir) ;
	r = scan_mode(dir, S_IFDIR) ;
	if(!r){
		strcpy(parentdir,dir) ;
		slash = get_rlen_until(dir,'/',dstlen) ;
		parentdir[slash] = '\0' ;
		r = dir_create_under(parentdir,dir+slash+1,mode) ;//+1 to remove the slash
		if (!r) return 0 ;
	}
	if(r < 0) return -1 ;
	strcpy(dir,dst) ;
	str_append_check(dir,'/') ;
	strcat(dir,name) ;
	r = dir_create(dir,mode) ;
	if(!r) return 0 ;
	return 1 ;
}
