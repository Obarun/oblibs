/* 
 * file_get_size.c
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
 
 
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/files.h>


ssize_t file_get_size(const char* filename)
{
	struct stat st;
	errno = 0 ;
	if (stat(filename, &st) == -1) return -1 ;
	return st.st_size;
}
