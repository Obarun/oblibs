/* 
 * get_len_until.c
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
#include <string.h>

#include <oblibs/string.h>

ssize_t get_len_until(const char *str, char const end)
{
	unsigned int i ;
    size_t len ;
    len = strlen(str) ;
    for (i = 0; i<=len; i++)
    	if (str[i] == end) return i ;
    return -1 ;
}
