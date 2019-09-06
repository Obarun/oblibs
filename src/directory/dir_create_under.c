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
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(name) ;
	
	char dir[dstlen + namelen + 2 + 1] ;
	memcpy(dir,dst,dstlen) ;
	dir[dstlen] = '/' ;
	memcpy(dir + dstlen + 1,name,namelen) ;
	dir[dstlen + 1 + namelen] = 0 ;
	return dir_create_parent(dir,mode) ;
}
