/* 
 * scan_mode.c
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

#include <oblibs/types.h>

#include <sys/stat.h>


ssize_t scan_mode (char const *str,mode_t mode)
{

	struct stat st ;	
	
	if(stat(str,&st)<0)	return 0 ;
	if(!((st.st_mode & S_IFMT) == mode)) return -1 ;
	
	return 1 ;
}
