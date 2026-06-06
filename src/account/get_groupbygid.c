/*
 * get_groupbygid.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/account.h>

#include <sys/types.h>
#include <grp.h>
#include <errno.h>

#include <oblibs/strbuf.h>

int get_groupbygid(gid_t gid,strbuf *name)
{
    int e = errno ;
    errno = 0 ;
    struct group *gr = getgrgid(gid);
    if (!gr)
    {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    strbuf_cats(name,gr->gr_name) ;
    errno = e ;
    return 1 ;
}




