/*
 * environ_clean_envfile_nunexport.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

#include <string.h>
#include <unistd.h>

#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int environ_clean_envfile_unexport(stralloc *modifs,char const *path)
{
    if (!environ_clean_envfile(modifs,path))
        return 0 ;

    if (!environ_remove_unexport(modifs,modifs) ||
        !sastr_split_string_in_nline(modifs))
            return 0 ;

    return 1 ;
}
