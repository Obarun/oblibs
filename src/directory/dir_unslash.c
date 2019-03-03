/* 
 * dir_unslash.c
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

#include <sys/types.h>

#include <oblibs/string.h>

void dir_unslash (char *str)
{
    size_t len = strlen(str) ;

    if (len <= 1)
        return ;
    --len ;
    if (str[len] == '/')
        str[len] = '\0' ;
}
