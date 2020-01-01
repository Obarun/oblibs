/* 
 * dir_create.c
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

#include <oblibs/directory.h>

#include <sys/types.h>
#include <string.h>
#include <errno.h>

int dir_create (char const *dst, mode_t mode)
{
	int e = errno ;
	errno = 0 ;
	if (dst[0] != '/')
	{
		errno = ENOTDIR ;
		return 0 ;
	}
	if (mkdir(dst, mode) < 0) return 0 ;
	errno = e ;
	return 1 ;
}
