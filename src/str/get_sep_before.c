/* 
 * get_sep_before.c
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

#include <sys/types.h>

#include <oblibs/string.h>

ssize_t get_sep_before (char const *line, char const sepstart, char const sepend)
{
	size_t linend, linesep ;
	linesep=get_len_until(line,sepstart) ;
	linend=get_len_until(line,sepend) ;
	if (linesep > linend) return -1 ;
	if (!linend) return 0 ;
	return linesep ;
}
