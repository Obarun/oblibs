/* 
 * environ_get_envfile_error.c
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

#include <oblibs/environ.h>

#include <skalibs/strerr2.h>

void environ_get_envfile_error(int error,char const *envdir)
{
	switch(error)
	{
		case -7: strerr_warnw2x("invalid environment: ",envdir) ;
		case -6: strerr_warnw2x("environment too long: ",envdir) ;
		case -5: strerr_warnwu2sys("open: ",envdir) ;
		case -4: strerr_warnwu2x("parse and clean: ",envdir) ;
		case -3: strerr_warnwu2x("get number of line: ",envdir) ;
		case -2: strerr_warnw2x("too many variable in: ",envdir) ;
		case -1: strerr_warnw2x("too many files in: ",envdir) ;
		case 0: strerr_warnwu1x("merge environment") ;
	}
}
