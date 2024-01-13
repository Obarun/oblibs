/*
 * get_namebyuid.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>

int get_namebyuid(uid_t uid,stralloc *name)
{
    int e = errno ;
    errno = 0 ;
    struct passwd *pw = getpwuid(uid);
    if (!pw)
    {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    stralloc_cats(name,pw->pw_name) ;
    errno = e ;
    return 1 ;
}




