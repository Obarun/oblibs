/* 
 * get_uidbyname.c
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

#include <oblibs/types.h>

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>



int get_uidbyname(const char *name,uid_t *uid)
{
	struct passwd *pw = getpwnam(name); 
    if (!pw)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
    *uid = pw->pw_uid ;
   
    return 1 ;
}


