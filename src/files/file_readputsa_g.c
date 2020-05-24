/* 
 * file_readputsa_g.c
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

#include <stddef.h> 

#include <oblibs/string.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>

int file_readputsa_g(stralloc *sa,char const *file)
{
	size_t base = sa->len, filelen = strlen(file) ;
	int wasnull = !sa->s ;
	char bname[filelen + 1] ;
	char dname[filelen + 1] ;
	if (!ob_basename(bname,file)) goto err ;
	if (!ob_dirname(dname,file)) goto err ;
	
	return file_readputsa(sa,dname,bname) ;
	err:
		if (wasnull) stralloc_free(sa) ;
		else sa->len = base ;
		return 0 ;
}
