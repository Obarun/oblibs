/* 
 * dir_create.c
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

int dir_create (char const *dst, mode_t mode)
{
	int fd ;
	fd = dir_scan_absopath(dst) ;
	if(fd<0) return 0 ;
	if (mkdir(dst, mode) < 0) return 0 ;
	return 1 ;
}
