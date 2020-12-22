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
        case -9: log_warn("sort environment: ",envdir) ; break ;
        case -8: log_warn("empty environment: ",envdir) ; break ;
        case -7: log_warn("invalid environment: ",envdir) ; break ;
        case -6: log_warn("environment too long: ",envdir) ; break ;
        case -5: log_warnusys("open: ",envdir) ; break ;
        case -4: log_warnu("parse and clean: ",envdir) ; break ;
        case -3: log_warnu("get number of line: ",envdir) ; break ;
        case -2: log_warn("too many variable in: ",envdir) ; break ;
        case -1: log_warn("too many files in: ",envdir) ; break ;
        case 0:  log_warnu("merge environment") ; break ;
    }
}
