/* 
 * sastr_dir_get_recursive.c
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

#include <oblibs/sastr.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#include <skalibs/stralloc.h>
#include <skalibs/direntry.h>
#include <skalibs/unix-transactional.h>

int sastr_dir_get_recursive(stralloc *sa, char const *srcdir,char const *exclude,mode_t mode) 
{
	size_t base = sa->len ;
	int wasnull = !sa->s ;
	int fdsrc ;
	
	stralloc subdir = STRALLOC_ZERO ;
	
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
			goto err ;
		
		if (!strcmp(d->d_name,exclude)) continue ;
		if (S_ISDIR(st.st_mode))
		{
			if (!auto_stra(&subdir,srcdir,"/",d->d_name,"/")) goto err ;
			if (!sastr_dir_get_recursive(sa,subdir.s,exclude,mode)) goto err ;
			subdir.len = 0 ;
		}
		mode_t r = get_flags(mode,st.st_mode) ;
		if (r > 0) 
		{
			if (!stralloc_cats(sa,srcdir)) goto err ;		 
			if (!stralloc_cats(sa,"/")) goto err ;		 
			if (!stralloc_catb(sa,d->d_name,strlen(d->d_name) + 1)) goto err ;		 
		
		}
	}
	dir_close(dir) ;
	stralloc_free(&subdir) ;
	return 1 ;
	err:
		dir_close(dir) ;
		if (wasnull) stralloc_free(sa) ;
		else sa->len = base ;
		stralloc_free(&subdir) ;
		return 0 ;
}
