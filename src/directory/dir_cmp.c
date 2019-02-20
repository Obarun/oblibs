/* 
 * dir_cmp.c
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

#include <string.h>
#include <sys/stat.h>


#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/stralist.h>

#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/genalloc.h>

int dir_cmp(char const *src, char const *dst,char const *exclude, genalloc *ga)
{
	
	int fdsrc,r ;
	
	DIR *dir = opendir(src) ;
	
	if (!dir) return 0 ;
	
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
		if (stat_at(fdsrc, d->d_name, &st) < 0) goto err ;
		
		if (obstr_equal(d->d_name,exclude)) continue ;
		
		r = dir_search(dst,d->d_name,S_IFDIR) ;
		if(!r)
		{
			if (!stra_add(ga,d->d_name)) goto err ;
		}
	}
	
	dir_close(dir) ;
	
	return 1 ;
	err:
		dir_close(dir) ;
		return 0 ;
}
