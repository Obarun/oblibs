/* 
 * file_readputsa.c
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
 

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/types.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <oblibs/directory.h>

#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>

int file_readputsa(stralloc *sa,char const *src,char const *file)
{
	int r ;
	
	size_t base = sa->len ;
	int wasnull = !sa->s ;
	size_t srclen = strlen(src) ;
	size_t filen = strlen(file) ;
	
	char tmp[srclen + 1 + filen + 1] ;
	
	auto_strings(tmp,src,"/",file) ;
	
	r = scan_mode(tmp,S_IFREG) ;
	if (r)
	{			
		size_t filesize=file_get_size(tmp) ;
		r = openreadfileclose(tmp,sa,filesize) ;
		if(!r) { errno = ENOENT ; goto err ; }
		if (!stralloc_0(sa)) goto err ;
		sa->len-- ;
		return 1 ;
	}
	
	errno = ENOENT ;

	err:
		if (wasnull) stralloc_free(sa) ;
		else sa->len = base ;
		return 0 ;
}
