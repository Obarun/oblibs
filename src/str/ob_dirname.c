/*
 * ob_dirname.c
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
 * */

#include <oblibs/string.h>

int ob_dirname(char *dst,char const *path)
{
    size_t i ;
    size_t pathlen = strlen(path) ;
    char name[pathlen+1] ;
    if (!ob_basename(name,path)) return 0 ;
    size_t namelen = strlen(name) ;
    i = (path[pathlen-namelen-1] == '/') ? 0 : 1 ;
    memcpy(dst,path,pathlen-namelen-i) ;
    dst[pathlen-namelen-i] = 0 ;
    return 1 ;
}
