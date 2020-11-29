/*
 * dir_is_child.c
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

#include <oblibs/directory.h>

#include <string.h>

void dir_unslash (char *path)
{
    size_t len = strlen (path) ;

    if (len <= 1)
        return ;

    --len ;

    if (path[len] == '/')
        path[len] = '\0' ;
}
