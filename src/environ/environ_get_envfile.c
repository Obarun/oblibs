/* 
 * environ_get_envfile.c
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

#include <oblibs/environ.h>

#include <string.h>
#include <oblibs/types.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

int environ_get_envfile(stralloc *modifs,char const *src)
{
	int r, nfile = 0, nvar = 0, e = 0 ;
	size_t filesize, pos = 0 ;
	stralloc sa = STRALLOC_ZERO ;
	stralloc toparse = GENALLOC_ZERO ;
	r = scan_mode(src,S_IFDIR) ;
	if (r < 0)
	{ 
		r = scan_mode(src,S_IFREG) ;
		if (!r || r < 0) { e = -7 ; goto err ; }
		filesize=file_get_size(src) ;
		if (filesize > MAXENV){ e = -6 ; goto err ; }
		if (!openreadfileclose(src,&sa,filesize)){ e = -5 ; goto err ; }
		r = environ_clean_envfile(modifs,&sa) ;
		if (!r) { e = -4 ; goto err ; }
		if (r == -1) { e = -8 ; goto err ; }
		if (!stralloc_catb(modifs,"\n",1)) goto err ;
	}
	else if (!r)
	{ 
		e = -7 ; 
		goto err ; 
	}
	else
	{
		r = sastr_dir_get(&toparse,src,"",S_IFREG) ;
		if (!r){ e = -5 ; goto err ; }
		for (;pos < toparse.len; pos += strlen(toparse.s + pos) + 1)
		{
			sa.len = 0 ;
			nfile++;
			if (nfile > MAXFILE){ e = -1 ; goto err ; }
			if (!file_readputsa(&sa,src,toparse.s+pos)){ e = -5 ; goto err ; }
			if (sa.len > MAXENV){ e = -6 ; goto err ; }
			r = environ_clean_envfile(modifs,&sa) ;
			if (!r) { e = -4 ; goto err ; }
			if (r == -1) { e = -8 ; goto err ; }
			if (!stralloc_catb(modifs,"\n",1)) goto err ;
		}
	}
	nvar = environ_get_num_of_line(&sa) ;
	if (nvar == -1) { e = -3 ; goto err ; }
	// --nvar to remove the last '\n' 
	if (--nvar > MAXVAR) { e = -2 ; goto err ; }
	
	stralloc_free(&sa) ;
	stralloc_free(&toparse) ;
	return 1 ;
	err:
		stralloc_free(&sa) ;
		stralloc_free(&toparse) ;
		return e ;
}
