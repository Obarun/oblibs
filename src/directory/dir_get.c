/* 
 * dir_get.c
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
 
#include <oblibs/directory.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#include <oblibs/stralist.h>
#include <oblibs/string.h>

#include <skalibs/direntry.h>
#include <skalibs/genalloc.h>
#include <skalibs/unix-transactional.h>


int dir_get(genalloc *ga, char const *srcdir,char const *exclude,mode_t mode)
{
	int fdsrc ;
	
	DIR *dir = opendir(srcdir) ;
	if (!dir)
		return 0 ;
	
	fdsrc = dir_fd(dir) ;
	
	for (;;)
    {
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
		if (stat_at(fdsrc, d->d_name, &st) < 0)
			return 0 ;
		if((st.st_mode & S_IFMT) == mode)
		{
			if (obstr_equal(d->d_name,exclude)) continue ; 
			if (!stra_add(ga,d->d_name)) return 0 ;		 
		}
	}

	return 1 ;
}
