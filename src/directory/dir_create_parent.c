/*
 * dir_create_parent.c
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

#include <oblibs/directory.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static int auto_create(char const *dst,mode_t mode)
{
    if (mkdir(dst, mode) < 0)
        if (errno != EEXIST) return 0 ;
    return 1 ;
}

int dir_create_parent(char const *dst, mode_t mode)
{
    size_t n = strlen(dst), i = 0 ;
    char tmp[n + 1] ;
    for (; i < n ; i++)
    {
        if ((dst[i] == '/') && i)
        {
            tmp[i] = 0 ;
            if (!auto_create(tmp,mode)) return 0 ;
        }
        tmp[i] = dst[i] ;
    }
    if (!auto_create(dst,mode)) return 0 ;
    return 1 ;
}
