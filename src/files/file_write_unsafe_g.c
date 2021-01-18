/*
 * file_write_unsafe_g.c
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

#include <oblibs/string.h>

#include <string.h>

#include <skalibs/djbunix.h>

int file_write_unsafe_g(char const *file,char const *content)
{
    size_t contlen = strlen(content) ;

    if (!openwritenclose_unsafe(file,content,contlen)) return 0 ;

    return 1 ;
}
