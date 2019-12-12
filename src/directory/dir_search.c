/* 
 * dir_search.c
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

#include <sys/types.h> //mode_t
#include <errno.h>

#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/direntry.h>
#include <skalibs/stralloc.h>

ssize_t dir_search (char const *srcdir, char const *search,mode_t mode)
{

	ssize_t r ;
	int e = 0 ;
	stralloc dst = STRALLOC_ZERO ;

	DIR *dir = opendir(srcdir) ;
	if (!dir){
		errno = ENOENT ;
		goto out ;
	}
	for (;;)
    {
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
		
		if (obstr_equal(d->d_name,search))
		{
			if(!stralloc_cats(&dst,srcdir)) goto err ;
			if(!stralloc_cats(&dst,"/")) goto err ;
			if(!stralloc_cats(&dst,search)) goto err ;
			if(!stralloc_0(&dst)) goto err ;
			r = scan_mode(dst.s,mode) ;
			if (!r)	{ errno = ENOENT ; goto err ; }
			if (r < 0){
				e = -1 ;
				errno = ENOENT ;
				goto err ;
			}
			e = 1 ;
			goto err ;
		}  
	}
	
	errno = ENOENT ;
	
	err:
		dir_close(dir) ;
	out:
		stralloc_free(&dst) ;
		return e ;
}
