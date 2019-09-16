/* 
 * file_create_empty.c
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
 
#include <oblibs/files.h>

#include <string.h>
#include <fcntl.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <skalibs/djbunix.h>


int file_create_empty (char const *dst, char const *name,mode_t mode)
{
	int fd, r ;
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(name) ;
	char file[dstlen + namelen + 2 + 1] ;
	r = dir_scan_absopath(dst) ;
	if(r<0) return 0 ;
	memcpy(file,dst,dstlen) ;
	file[dstlen] = '\0' ;
	str_append_check(file,'/') ;
	strcat(file,name) ;
	fd = open3 (file, O_WRONLY | O_NONBLOCK | O_CREAT | O_EXCL, mode) ;
	if (fd<0){
		fd_close(fd) ;
		return 0;
	}
	fd_close(fd) ;
	return 1 ;
}
