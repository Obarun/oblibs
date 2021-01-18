/*
 * get_flags.c
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

#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/types.h>

mode_t get_flags(mode_t mode, mode_t stmode)
{
    int flags = stmode & S_IFMT ;
    if (mode & flags) return flags ;
    return 0 ;
}
