/* 
 * environ_get_envfile_error.c
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
#include <oblibs/log.h>

void environ_get_envfile_error(int error,char const *envdir)
{
	switch(error)
	{
		case -8: log_warn("empty environment: ",envdir) ;
		case -7: log_warn("invalid environment: ",envdir) ;
		case -6: log_warn("environment too long: ",envdir) ;
		case -5: log_warnusys("open: ",envdir) ;
		case -4: log_warnu("parse and clean: ",envdir) ;
		case -3: log_warnu("get number of line: ",envdir) ;
		case -2: log_warn("too many variable in: ",envdir) ;
		case -1: log_warn("too many files in: ",envdir) ;
		case 0:	 log_warnu("merge environment") ;
	}
}
