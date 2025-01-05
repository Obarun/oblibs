/*
 * file_write_unsafe.c
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

#include <oblibs/string.h>

#include <string.h>

#include <skalibs/djbunix.h>

int file_write_unsafe(char const *dst,char const *name,char const *content,size_t contlen)
{
    size_t dstlen = strlen(dst) ;
    size_t namelen = strlen(name) ;
    char file[dstlen + namelen + 1 + 1] ;

    auto_strings(file,dst,"/",name) ;

    if (!openwritenclose_unsafe(file,content,contlen)) return 0 ;

    return 1 ;
}
