/*
 * get_gidbyname.c
 *
 * Copyright (c) 2020 Eric Vidal <eric@obarun.org>
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
#include <pwd.h>
#include <errno.h>

#include <oblibs/account.h>

int get_gidbyname(char const *name,gid_t *gid)
{
    int e = errno ;
    errno = 0 ;
    struct passwd *pw = getpwnam(name) ;
    if (!pw)
    {
        if (!errno) errno = ESRCH ;
        return -1 ;
    }
    *gid = pw->pw_gid ;
    errno = e ;
    return 1 ;
}


