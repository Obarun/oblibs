/*
 * get_gidbygroup.c
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
#include <grp.h>
#include <errno.h>

#include <skalibs/stralloc.h>

int get_gidbygroup(char const *name,gid_t *gid)
{
    int e = errno ;
    errno = 0 ;
    struct group *gr = getgrnam(name) ;
    if (!gr)
    {
        if (!errno) errno = ESRCH ;
        return -1 ;
    }
    *gid = gr->gr_gid ;
    errno = e ;
    return 1 ;
}




