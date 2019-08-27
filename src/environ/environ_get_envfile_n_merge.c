/* 
 * environ_get_envfile_n_merge.c
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

#include <sys/types.h>

#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>

int environ_get_envfile_n_merge(char const *src,char const *const *envp,char const **newenv, char *tmpenv)
{
	int e = 0 ;
	stralloc modifs = STRALLOC_ZERO ;
	size_t envlen = env_len(envp) ;
	
	e = environ_get_envfile(&modifs,src) ;
	if (e < 1) goto err ;
	if (!sastr_split_string_in_nline(&modifs)) goto err ;
	size_t n = env_len(envp) + 1 + byte_count(modifs.s,modifs.len,'\0') ;
	size_t mlen = modifs.len ;
	memcpy(tmpenv,modifs.s,mlen) ;
	tmpenv[mlen] = 0 ;
	
	if (!env_merge(newenv, n, envp, envlen, tmpenv, mlen)) goto err ;
	
	stralloc_free(&modifs) ;
	return 1 ;
	err:
		stralloc_free(&modifs) ;
		return e ;
}
