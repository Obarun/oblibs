/*
 * ob_basename.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

int ob_basename(char *dst,char const *path)
{
    size_t slen = strlen(path) ;
    if (path[slen-1] == '/') slen-- ;
    ssize_t pathlen = get_rlen_until(path,'/',slen) ;
    if (pathlen <= 0) return 0 ;
    pathlen++ ;
    size_t namelen = slen - pathlen ;
    memcpy(dst,path+pathlen,namelen) ;
    dst[namelen] = 0 ;
    return 1 ;
}
